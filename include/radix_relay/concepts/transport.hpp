#pragma once

#include <span>
#include <string_view>

namespace radix_relay::concepts {

template<typename T>
concept transport = requires(T transport, const std::span<const std::byte> payload, const std::string_view address) {
  transport.connect(address);
  transport.send(payload);
};

}// namespace radix_relay::concepts
