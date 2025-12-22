#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <fmt/core.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <variant>

namespace radix_relay::core {

/**
 * @brief Handles presentation events and generates user-facing messages.
 *
 * Converts presentation events (messages received, sessions established, etc.)
 * into formatted display messages for the user interface.
 */
struct presentation_handler
{
  // Type traits for standard_processor
  using in_queue_t = async::async_queue<events::presentation_event_variant_t>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::display_message>> display;
  };

  /**
   * @brief Constructs a presentation handler.
   *
   * @param queues Output queues for display messages
   */
  explicit presentation_handler(const out_queues_t &queues) : display_out_queue_(queues.display) {}

  /**
   * @brief Variant handler for standard_processor.
   *
   * @param event Presentation event variant
   */
  auto handle(const events::presentation_event_variant_t &event) const -> void
  {
    std::visit([this](const auto &evt) { this->handle(evt); }, event);
  }

  /**
   * @brief Handles a received encrypted message event.
   *
   * @param evt Message received event
   */
  auto handle(const events::message_received &evt) const -> void
  {
    const auto &sender_display = evt.sender_alias.empty() ? evt.sender_rdx : evt.sender_alias;
    emit("Message from {}: {}\n", sender_display, evt.content);
  }

  /**
   * @brief Handles a session established event.
   *
   * @param evt Session established event
   */
  auto handle(const events::session_established &evt) const -> void
  {
    emit("Encrypted session established with {}\n", evt.peer_rdx);
  }

  /**
   * @brief Handles a bundle announcement received event.
   *
   * @param evt Bundle announcement received event
   */
  static auto handle(const events::bundle_announcement_received &evt) -> void
  {
    spdlog::debug("Received bundle announcement from {}", evt.pubkey);
  }

  /**
   * @brief Handles a bundle announcement removed event.
   *
   * @param evt Bundle announcement removed event
   */
  static auto handle(const events::bundle_announcement_removed &evt) -> void
  {
    spdlog::debug("Bundle announcement removed for {}", evt.pubkey);
  }

  /**
   * @brief Handles a message sent event.
   *
   * @param evt Message sent event
   */
  auto handle(const events::message_sent &evt) const -> void
  {
    if (evt.accepted) {
      emit("Message sent to {}\n", evt.peer);
    } else {
      emit("Failed to send message to {}\n", evt.peer);
    }
  }

  /**
   * @brief Handles a bundle published event.
   *
   * @param evt Bundle published event
   */
  auto handle(const events::bundle_published &evt) const -> void
  {
    if (evt.accepted) {
      emit("Identity bundle published (event: {})\n", evt.event_id);
    } else {
      emit("Failed to publish identity bundle\n");
    }
  }

  /**
   * @brief Handles a subscription established event.
   *
   * @param evt Subscription established event
   */
  static auto handle(const events::subscription_established &evt) -> void
  {
    spdlog::debug("Subscription established: {}", evt.subscription_id);
  }

  /**
   * @brief Handles an identities listed event.
   *
   * @param evt Identities listed event
   */
  auto handle(const events::identities_listed &evt) const -> void
  {
    if (evt.identities.empty()) {
      emit("No identities discovered yet\n");
    } else {
      emit("Discovered identities:\n");
      for (const auto &identity : evt.identities) {
        emit("  {} (nostr: {})\n", identity.rdx_fingerprint, identity.nostr_pubkey);
      }
    }
  }

private:
  std::shared_ptr<async::async_queue<events::display_message>> display_out_queue_;

  template<typename... Args> auto emit(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    display_out_queue_->push(events::display_message{ fmt::format(format_string, std::forward<Args>(args)...) });
  }
};

}// namespace radix_relay::core
