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

struct websocket_connection_params
{
  std::string_view host;
  std::string_view port;
  std::string_view path;
};

class websocket_stream
{
private:
  static constexpr int connection_timeout_seconds = 30;

  boost::asio::ssl::context ssl_context_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
  boost::beast::flat_buffer read_buffer_;

public:
  explicit websocket_stream(const std::shared_ptr<boost::asio::io_context> &io_context);

  auto async_connect(websocket_connection_params params,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_write(std::span<const std::byte> data,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_read(const boost::asio::mutable_buffer &buffer,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;
};


}// namespace radix_relay::transport
