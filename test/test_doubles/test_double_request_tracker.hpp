#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <radix_relay/nostr_protocol.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace radix_relay_test {

class TestDoubleRequestTracker
{
public:
  mutable std::vector<std::string> tracked_ids;
  mutable std::vector<std::string> resolved_ids;

  auto track(std::string event_id,
    std::function<void(const radix_relay::nostr::protocol::ok &)> /*callback*/,
    std::chrono::milliseconds /*timeout*/) -> void
  {
    tracked_ids.push_back(std::move(event_id));
  }

  template<typename ResponseType> auto resolve(const std::string &event_id, const ResponseType & /*response*/) -> void
  {
    resolved_ids.push_back(event_id);
  }

  template<typename ResponseType = radix_relay::nostr::protocol::ok>
  auto async_track(std::string event_id, std::chrono::milliseconds /*timeout*/) -> boost::asio::awaitable<ResponseType>
  {
    tracked_ids.push_back(std::move(event_id));
    co_return ResponseType{};
  }
};

}// namespace radix_relay_test
