#pragma once

#include <radix_relay/concepts/websocket_stream.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>
#include <cstddef>
#include <functional>
#include <openssl/ssl.h>
#include <span>
#include <string>
#include <string_view>

namespace radix_relay {

class beast_websocket_stream
{
private:
  static constexpr int connection_timeout_seconds = 30;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  [[maybe_unused]] boost::asio::io_context &io_context_;
  boost::asio::ssl::context ssl_context_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
  boost::beast::flat_buffer read_buffer_;

public:
  explicit beast_websocket_stream(boost::asio::io_context &io_context);

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  auto async_connect(std::string_view host,
    std::string_view port,
    std::string_view path,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_write(std::span<const std::byte> data,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_read(boost::asio::mutable_buffer buffer,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;

  auto async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void;
};

inline beast_websocket_stream::beast_websocket_stream(boost::asio::io_context &io_context)
  : io_context_(io_context), ssl_context_(boost::asio::ssl::context::tlsv12_client), resolver_(io_context),
    ws_(boost::asio::make_strand(io_context), ssl_context_)
{
  ssl_context_.set_default_verify_paths();
  ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
inline auto beast_websocket_stream::async_connect(const std::string_view host,
  const std::string_view port,
  const std::string_view path,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  namespace beast = boost::beast;

  auto host_str = std::string(host);
  auto port_str = std::string(port);
  auto path_str = std::string(path);

  resolver_.async_resolve(host_str,
    port_str,
    [this, host_str, port_str, path_str, handler = std::move(handler)](const boost::system::error_code &error_code,
      const boost::asio::ip::tcp::resolver::results_type &results) mutable {
      if (error_code) {
        handler(error_code, 0);
        return;
      }

      beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(connection_timeout_seconds));

      beast::get_lowest_layer(ws_).async_connect(results,
        [this, host_str, path_str, handler = std::move(handler)](
          const boost::system::error_code &connect_error, const boost::asio::ip::tcp::endpoint & /*endpoint*/) mutable {
          if (connect_error) {
            handler(connect_error, 0);
            return;
          }

      // SSL_set_tlsext_host_name is a macro that performs old-style casts internally.
      // Suppress warnings from both clang-tidy and GCC.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-array-decay)
          if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_str.c_str())) {
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
            handler(boost::asio::error::operation_not_supported, 0);
            return;
          }

          ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
            [this, host_str, path_str, handler = std::move(handler)](
              const boost::system::error_code &ssl_error) mutable {
              if (ssl_error) {
                handler(ssl_error, 0);
                return;
              }

              beast::get_lowest_layer(ws_).expires_never();

              ws_.set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::client));
              ws_.set_option(beast::websocket::stream_base::decorator([](beast::websocket::request_type &req) {
                req.set(
                  boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " radix-relay");
              }));

              ws_.async_handshake(host_str,
                path_str,
                [handler = std::move(handler)](const boost::system::error_code &ws_error) { handler(ws_error, 0); });
            });
        });
    });
}

inline auto beast_websocket_stream::async_write(const std::span<const std::byte> data,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  ws_.async_write(boost::asio::buffer(data.data(), data.size()),
    [handler = std::move(handler)](const boost::system::error_code &error_code, std::size_t bytes_transferred) {
      handler(error_code, bytes_transferred);
    });
}

inline auto beast_websocket_stream::async_read(boost::asio::mutable_buffer buffer,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  read_buffer_.clear();
  ws_.async_read(read_buffer_,
    [this, buffer, handler = std::move(handler)](
      const boost::system::error_code &error_code, std::size_t /*bytes_transferred*/) {
      if (!error_code) {
        const auto data = read_buffer_.data();
        const std::size_t size = std::min(boost::asio::buffer_size(buffer), data.size());
        boost::asio::buffer_copy(buffer, data, size);
        handler(error_code, size);
      } else {
        handler(error_code, 0);
      }
    });
}

inline auto beast_websocket_stream::async_close(
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  ws_.async_close(boost::beast::websocket::close_code::normal,
    [handler = std::move(handler)](const boost::system::error_code &error_code) { handler(error_code, 0); });
}

static_assert(radix_relay::concepts::websocket_stream<radix_relay::beast_websocket_stream>);

}// namespace radix_relay
