#pragma once

#include <concepts>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <radix_relay/concepts/transport.hpp>

namespace radix_relay_test {

class TestDoubleNostrTransport
{
public:
  mutable std::vector<std::vector<std::byte>> sent_messages;
  mutable bool is_connected = false;
  mutable std::function<void(std::span<const std::byte>)> message_callback;

  auto connect(const std::string_view /*address*/) const -> void { is_connected = true; }

  auto send(const std::span<const std::byte> &message) const -> void
  {
    sent_messages.emplace_back(message.begin(), message.end());
  }

  auto register_message_callback(std::function<void(std::span<const std::byte>)> callback) const -> void
  {
    message_callback = callback;
  }
};

static_assert(radix_relay::concepts::Transport<TestDoubleNostrTransport>);

}// namespace radix_relay_test
