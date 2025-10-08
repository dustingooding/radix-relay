#pragma once

#include <chrono>
#include <concepts>
#include <functional>
#include <radix_relay/nostr_protocol.hpp>
#include <string>

namespace radix_relay::concepts {

template<typename T>
concept RequestTracker = requires(T tracker,
  const std::string &event_id,
  std::function<void(const nostr::protocol::ok &)> callback,
  std::chrono::milliseconds timeout,
  const nostr::protocol::ok &response) {
  tracker.track(event_id, callback, timeout);
  tracker.resolve(event_id, response);
};

}// namespace radix_relay::concepts
