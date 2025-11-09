#pragma once

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <concepts>
#include <cstddef>
#include <functional>
#include <span>

namespace radix_relay::transport {
struct websocket_connection_params;
}

namespace radix_relay::concepts {

template<typename T>
concept websocket_stream = requires(T &stream,
  const transport::websocket_connection_params params,
  const std::span<const std::byte> data,
  const boost::asio::mutable_buffer &buffer,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) {
  { stream.async_connect(params, handler) } -> std::same_as<void>;
  { stream.async_write(data, handler) } -> std::same_as<void>;
  { stream.async_read(buffer, handler) } -> std::same_as<void>;
  { stream.async_close(handler) } -> std::same_as<void>;
};

}// namespace radix_relay::concepts
