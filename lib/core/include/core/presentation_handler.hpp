#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <fmt/core.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace radix_relay::core {

struct presentation_handler
{
  explicit presentation_handler(const std::shared_ptr<async::async_queue<events::display_message>> &display_out_queue)
    : display_out_queue_(display_out_queue)
  {}

  auto handle(const events::message_received &evt) const -> void
  {
    const auto &sender_display = evt.sender_alias.empty() ? evt.sender_rdx : evt.sender_alias;
    emit("Message from {}: {}\n", sender_display, evt.content);
  }

  auto handle(const events::session_established &evt) const -> void
  {
    emit("Encrypted session established with {}\n", evt.peer_rdx);
  }

  static auto handle(const events::bundle_announcement_received &evt) -> void
  {
    spdlog::debug("Received bundle announcement from {}", evt.pubkey);
  }

  static auto handle(const events::bundle_announcement_removed &evt) -> void
  {
    spdlog::debug("Bundle announcement removed for {}", evt.pubkey);
  }

  auto handle(const events::message_sent &evt) const -> void
  {
    if (evt.accepted) {
      emit("Message sent to {}\n", evt.peer);
    } else {
      emit("Failed to send message to {}\n", evt.peer);
    }
  }

  auto handle(const events::bundle_published &evt) const -> void
  {
    if (evt.accepted) {
      emit("Identity bundle published (event: {})\n", evt.event_id);
    } else {
      emit("Failed to publish identity bundle\n");
    }
  }

  static auto handle(const events::subscription_established &evt) -> void
  {
    spdlog::debug("Subscription established: {}", evt.subscription_id);
  }

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
