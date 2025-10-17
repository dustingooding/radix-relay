#pragma once

#include <boost/asio.hpp>
#include <concepts>
#include <span>
#include <string_view>
#include <vector>

#include <radix_relay/concepts/transport.hpp>

namespace radix_relay_test {

class TestDoubleNostrTransport
{
public:
  boost::asio::io_context &io_context_;
  mutable std::vector<std::vector<std::byte>> sent_messages;
  mutable bool is_connected = false;

  explicit TestDoubleNostrTransport(boost::asio::io_context &io_context) : io_context_(io_context) {}

  [[nodiscard]] auto io_context() -> boost::asio::io_context & { return io_context_; }

  auto connect(const std::string_view /*address*/) const -> void { is_connected = true; }

  auto send(const std::span<const std::byte> &message) const -> void
  {
    sent_messages.emplace_back(message.begin(), message.end());
  }
};

static_assert(radix_relay::concepts::transport<TestDoubleNostrTransport>);

}// namespace radix_relay_test
