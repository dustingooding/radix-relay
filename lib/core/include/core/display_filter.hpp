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
 * Outputs unified UI event stream containing both display messages and control events.
 * - enter_chat_mode: UI should update to show chat mode
 * - exit_chat_mode: UI should exit chat mode
 * - display_message: Filtered text messages
 *
 * Filtering logic:
 * - System messages and command feedback always pass through
 * - In chat mode: only messages from active contact are displayed
 * - Outside chat mode: all messages pass through
 */
struct display_filter
{
  // Type traits for standard_processor
  using in_queue_t = async::async_queue<events::display_filter_input_t>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::ui_event_t>> ui;
  };

  /**
   * @brief Constructs a display filter with the given output queue.
   *
   * @param queues Output queue for unified UI events
   */
  explicit display_filter(const out_queues_t &queues) : ui_queue_(queues.ui) {}

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
  auto handle(const events::enter_chat_mode &evt) const -> void
  {
    active_chat_rdx_ = evt.rdx_fingerprint;
    ui_queue_->push(evt);
  }

  /**
   * @brief Handles exiting chat mode.
   *
   * @param evt Exit chat mode event
   */
  auto handle(const events::exit_chat_mode &evt) const -> void
  {
    active_chat_rdx_.reset();
    ui_queue_->push(evt);
  }

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
      ui_queue_->push(msg);
      return;
    }

    // Not in chat mode: pass everything through
    if (not active_chat_rdx_.has_value()) {
      ui_queue_->push(msg);
      return;
    }

    // In chat mode: only pass messages associated with active contact
    if (msg.contact_rdx.has_value() and msg.contact_rdx.value() == active_chat_rdx_.value()) { ui_queue_->push(msg); }
    // Messages not matching active contact are discarded (already stored in history)
  }

private:
  std::shared_ptr<async::async_queue<events::ui_event_t>> ui_queue_;

  // Chat context state (only accessed by this processor's thread, no mutex needed)
  mutable std::optional<std::string> active_chat_rdx_;
};

}// namespace radix_relay::core
