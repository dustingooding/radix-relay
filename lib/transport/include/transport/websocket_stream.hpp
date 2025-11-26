#pragma once


#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

namespace radix_relay::transport {

/**
 * @brief Parameters for establishing a WebSocket connection.
 */
struct websocket_connection_params
{
  std::string_view host;///< Hostname or IP address
  std::string_view port;///< Port number (typically "443" for wss://)
  std::string_view path;///< WebSocket path (e.g., "/" or "/api/v1")
};

/**
 * @brief WebSocket stream with TLS support.
 *
 * Provides asynchronous operations for secure WebSocket connections using Boost.Beast.
 */
class websocket_stream
{
private:
  static constexpr int connection_timeout_seconds = 30;

  boost::asio::ssl::context ssl_context_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
  boost::beast::flat_buffer read_buffer_;

public:
  /**
   * @brief Constructs a WebSocket stream.
   *
   * @param io_context Boost.Asio io_context for async operations
   */
  explicit websocket_stream(const std::shared_ptr<boost::asio::io_context> &io_context);

  /**
   * @brief Asynchronously connects to a WebSocket endpoint.
   *
   * @param params Connection parameters (host, port, path)
   * @param handler Completion handler called with error code and bytes transferred
   */
  auto async_connect(websocket_connection_params params,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  /**
   * @brief Asynchronously writes data to the WebSocket.
   *
   * @param data Data bytes to write
   * @param handler Completion handler called with error code and bytes transferred
   */
  auto async_write(std::span<const std::byte> data,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  /**
   * @brief Asynchronously reads data from the WebSocket.
   *
   * @param buffer Buffer to store received data
   * @param handler Completion handler called with error code and bytes transferred
   */
  auto async_read(const boost::asio::mutable_buffer &buffer,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  /**
   * @brief Asynchronously closes the WebSocket connection.
   *
   * @param handler Completion handler called with error code and bytes transferred
   */
  auto async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;
};


}// namespace radix_relay::transport
