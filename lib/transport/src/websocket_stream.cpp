#include <transport/websocket_stream.hpp>

#include <chrono>

namespace radix_relay::transport {

websocket_stream::websocket_stream(boost::asio::io_context &io_context)
  : ssl_context_(boost::asio::ssl::context::tlsv12_client), resolver_(io_context),
    ws_(boost::asio::make_strand(io_context), ssl_context_)
{
  ssl_context_.set_default_verify_paths();
  ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
}

auto websocket_stream::async_connect(websocket_connection_params params,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  namespace beast = boost::beast;

  auto host_str = std::string(params.host);
  auto port_str = std::string(params.port);
  auto path_str = std::string(params.path);

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

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,hicpp-no-array-decay)
          if (not SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_str.c_str())) {
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

auto websocket_stream::async_write(const std::span<const std::byte> data,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  ws_.async_write(boost::asio::buffer(data.data(), data.size()),
    [handler = std::move(handler)](const boost::system::error_code &error_code, std::size_t bytes_transferred) {
      handler(error_code, bytes_transferred);
    });
}

auto websocket_stream::async_read(const boost::asio::mutable_buffer &buffer,
  std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  read_buffer_.clear();
  ws_.async_read(read_buffer_,
    [this, buffer, handler = std::move(handler)](
      const boost::system::error_code &error_code, std::size_t /*bytes_transferred*/) {
      if (not error_code) {
        const auto data = read_buffer_.data();
        const std::size_t size = std::min(boost::asio::buffer_size(buffer), data.size());
        boost::asio::buffer_copy(buffer, data, size);
        handler(error_code, size);
      } else {
        handler(error_code, 0);
      }
    });
}

auto websocket_stream::async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
{
  ws_.async_close(boost::beast::websocket::close_code::normal,
    [handler = std::move(handler)](const boost::system::error_code &error_code) { handler(error_code, 0); });
}

}// namespace radix_relay::transport
