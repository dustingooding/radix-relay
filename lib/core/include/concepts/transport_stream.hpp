#pragma once

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <concepts>
#include <cstddef>
#include <functional>
#include <span>

namespace radix_relay::concepts {

/**
 * @brief Concept defining the interface for transport stream operations.
 *
 * Types satisfying this concept provide async operations for byte stream connections,
 * including connect, read, write, and close. This abstraction works for any transport
 * mechanism (WebSocket, BLE, etc.).
 *
 * Each stream type must define a `connection_params_t` type alias specifying the
 * connection parameters type it requires (e.g., websocket_connection_params, ble_connection_params).
 */
template<typename T>
concept transport_stream = requires(T &stream,
  typename T::connection_params_t params,
  const std::span<const std::byte> data,
  const boost::asio::mutable_buffer &buffer,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) {
  typename T::connection_params_t;
  { stream.async_connect(params, handler) } -> std::same_as<void>;
  { stream.async_write(data, handler) } -> std::same_as<void>;
  { stream.async_read(buffer, handler) } -> std::same_as<void>;
  { stream.async_close(handler) } -> std::same_as<void>;
};

}// namespace radix_relay::concepts
