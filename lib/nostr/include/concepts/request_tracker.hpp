#pragma once

#include <boost/asio/awaitable.hpp>
#include <chrono>
#include <concepts>
#include <functional>
#include <nostr/protocol.hpp>
#include <string>

namespace radix_relay::concepts {

template<typename T>
concept request_tracker = requires(T tracker,
  const std::string &event_id,
  std::function<void(const nostr::protocol::ok &)> callback,
  std::chrono::milliseconds timeout,
  const nostr::protocol::ok &response,
  const nostr::protocol::eose &eose_response) {
  tracker.track(event_id, callback, timeout);
  tracker.resolve(event_id, response);
  tracker.resolve(event_id, eose_response);
  {
    tracker.template async_track<nostr::protocol::ok>(event_id, timeout)
  } -> std::same_as<boost::asio::awaitable<nostr::protocol::ok>>;
  {
    tracker.template async_track<nostr::protocol::eose>(event_id, timeout)
  } -> std::same_as<boost::asio::awaitable<nostr::protocol::eose>>;
};

}// namespace radix_relay::concepts
