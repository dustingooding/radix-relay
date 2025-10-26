#pragma once

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <concepts>
#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

namespace radix_relay::concepts {

template<typename T>
concept websocket_stream = requires(T &stream,
  const std::string_view host,
  const std::string_view port,
  const std::string_view path,
  const std::span<const std::byte> data,
  boost::asio::mutable_buffer buffer,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) {
  { stream.async_connect(host, port, path, handler) } -> std::same_as<void>;
  { stream.async_write(data, handler) } -> std::same_as<void>;
  { stream.async_read(buffer, handler) } -> std::same_as<void>;
  { stream.async_close(handler) } -> std::same_as<void>;
};

}// namespace radix_relay::concepts
