#pragma once

#include <concepts>
#include <functional>
#include <span>
#include <string_view>

namespace radix_relay::concepts {

template<typename T>
concept Transport = requires(T transport,
  const std::span<const std::byte> payload,
  const std::string_view address,
  std::function<void(std::span<const std::byte>)> callback) {
  transport.connect(address);
  transport.send(payload);
  transport.register_message_callback(callback);
};

}// namespace radix_relay::concepts
