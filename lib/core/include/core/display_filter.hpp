#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace radix_relay::core {

/**
 * @brief Filters display messages based on active chat context.
 *
 * Controlled via events:
 * - enter_chat_mode: Start filtering to show only one contact
 * - exit_chat_mode: Stop filtering, show all messages
 * - display_message: Filter and forward based on current mode
 *
 * Routes display messages through filtering logic:
 * - System messages always pass through
 * - In chat mode: only messages from active contact are displayed
 * - Outside chat mode: all messages pass through
 */
struct display_filter
{
  // Type traits for standard_processor
  using in_queue_t = async::async_queue<events::display_filter_input_t>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::display_message>> filtered;
  };

  /**
   * @brief Constructs a display filter with the given output queue.
   *
   * @param queues Output queues for filtered display messages
   */
  explicit display_filter(const out_queues_t &queues) : filtered_queue_(queues.filtered) {}

  /**
   * @brief Variant handler for standard_processor.
   *
   * @param input Filter input variant (message or control event)
   */
  auto handle(const events::display_filter_input_t &input) const -> void
  {
    std::visit([this](const auto &evt) { this->handle(evt); }, input);
  }

  /**
   * @brief Handles entering chat mode.
   *
   * @param evt Enter chat mode event
   */
  auto handle(const events::enter_chat_mode &evt) const -> void { active_chat_rdx_ = evt.rdx_fingerprint; }

  /**
   * @brief Handles exiting chat mode.
   *
   * @param evt Exit chat mode event
   */
  auto handle(const events::exit_chat_mode & /*evt*/) const -> void { active_chat_rdx_.reset(); }

  /**
   * @brief Handles a display message, filtering based on chat context.
   *
   * @param msg Display message to filter
   */
  auto handle(const events::display_message &msg) const -> void
  {
    // System messages and command feedback always pass through
    if (msg.source_type == events::display_message::source::system
        or msg.source_type == events::display_message::source::command_feedback) {
      filtered_queue_->push(msg);
      return;
    }

    // Not in chat mode: pass everything through
    if (not active_chat_rdx_.has_value()) {
      filtered_queue_->push(msg);
      return;
    }

    // In chat mode: only pass messages associated with active contact
    if (msg.contact_rdx.has_value() and msg.contact_rdx.value() == active_chat_rdx_.value()) {
      filtered_queue_->push(msg);
    }
    // Messages not matching active contact are discarded (already stored in history)
  }

private:
  std::shared_ptr<async::async_queue<events::display_message>> filtered_queue_;

  // Chat context state (only accessed by this processor's thread, no mutex needed)
  mutable std::optional<std::string> active_chat_rdx_;
};

}// namespace radix_relay::core
