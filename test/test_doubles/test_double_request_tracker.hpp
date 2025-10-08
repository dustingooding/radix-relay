#pragma once

#include <radix_relay/nostr_protocol.hpp>
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

  auto resolve(const std::string &event_id, const radix_relay::nostr::protocol::ok & /*response*/) -> void
  {
    resolved_ids.push_back(event_id);
  }
};

}// namespace radix_relay_test
