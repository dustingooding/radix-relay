#pragma once

#include <fmt/core.h>
#include <memory>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/core/events.hpp>

namespace radix_relay::core {

struct transport_event_display_handler
{
  explicit transport_event_display_handler(
    std::shared_ptr<async::async_queue<events::display_message>> display_out_queue)// NOLINT(modernize-pass-by-value)
    : display_out_queue_(display_out_queue)// NOLINT(performance-unnecessary-value-param)
  {}

  auto handle(const events::message_received &evt) const -> void
  {
    emit("Message from {}: {}\n", evt.sender_rdx, evt.content);
  }

  auto handle(const events::session_established &evt) const -> void
  {
    emit("Encrypted session established with {}\n", evt.peer_rdx);
  }

  auto handle(const events::bundle_announcement_received &evt) const -> void
  {
    emit("Received bundle announcement from {}\n", evt.pubkey);
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

  auto handle(const events::subscription_established &evt) const -> void
  {
    emit("Subscription established: {}\n", evt.subscription_id);
  }

private:
  std::shared_ptr<async::async_queue<events::display_message>> display_out_queue_;

  template<typename... Args> auto emit(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    display_out_queue_->push(events::display_message{ fmt::format(format_string, std::forward<Args>(args)...) });
  }
};

}// namespace radix_relay::core
