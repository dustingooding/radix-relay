#pragma once

#include <radix_relay/concepts/websocket_stream.hpp>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace radix_relay::nostr {

template<concepts::websocket_stream WebSocketStream> class transport
{
public:
  using send_bytes_to_session_fn_t = std::function<void(std::vector<std::byte>)>;

private:
  bool connected_{ false };
  send_bytes_to_session_fn_t send_bytes_to_session_;
  std::shared_ptr<WebSocketStream> ws_;
  boost::asio::io_context &io_context_;
  static constexpr size_t read_buffer_size = 8192;
  std::array<std::byte, read_buffer_size> read_buffer_{};

  std::string host_;
  std::string port_;
  std::string path_;

  auto parse_url(const std::string_view address) -> void
  {
    std::string addr_str(address);

    port_ = "443";
    path_ = "/";

    static constexpr size_t wss_prefix_length = 6;

    if (addr_str.starts_with("ws://")) {
      throw std::runtime_error("Insecure WebSocket (ws://) not supported. Use wss:// for security.");
    }
    if (addr_str.starts_with("wss://")) {
      addr_str = addr_str.substr(wss_prefix_length);
      port_ = "443";
    } else if (!addr_str.starts_with("http")) {
      port_ = "443";
    }

    auto slash_pos = addr_str.find('/');
    if (slash_pos != std::string::npos) {
      host_ = addr_str.substr(0, slash_pos);
      path_ = addr_str.substr(slash_pos);
    } else {
      host_ = addr_str;
    }

    auto colon_pos = host_.find(':');
    if (colon_pos != std::string::npos) {
      port_ = host_.substr(colon_pos + 1);
      host_.resize(colon_pos);
    }
  }

  auto start_read() -> void
  {
    ws_->async_read(
      boost::asio::buffer(read_buffer_), [this](const boost::system::error_code &error, std::size_t bytes_transferred) {
        handle_read(error, bytes_transferred);
      });
  }

  auto handle_read(const boost::system::error_code &error, std::size_t bytes_transferred) -> void
  {
    if (!error and bytes_transferred > 0) {
      if (send_bytes_to_session_) {
        std::vector<std::byte> bytes(
          read_buffer_.begin(), read_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_transferred));
        send_bytes_to_session_(std::move(bytes));
      }
      start_read();
    } else {
      connected_ = false;
    }
  }

public:
  using connect_handler_t = std::function<void(const boost::system::error_code &)>;

  transport(std::shared_ptr<WebSocketStream> websocket_stream,
    boost::asio::io_context &io_context,
    send_bytes_to_session_fn_t send_bytes_to_session)
    : send_bytes_to_session_(std::move(send_bytes_to_session)), ws_(std::move(websocket_stream)),
      io_context_(io_context)
  {}

  ~transport() { disconnect(); }

  transport(const transport &) = delete;
  auto operator=(const transport &) -> transport & = delete;
  transport(transport &&) = delete;
  auto operator=(transport &&) -> transport & = delete;

  auto async_connect(const std::string_view address, connect_handler_t handler) -> void
  {
    parse_url(address);

    ws_->async_connect(host_,
      port_,
      path_,
      [this, handler = std::move(handler)](const boost::system::error_code &error_code, std::size_t /*bytes*/) {
        if (!error_code) {
          connected_ = true;
          start_read();
        }
        handler(error_code);
      });
  }

  auto send(std::span<const std::byte> payload) -> void
  {
    if (!connected_) { throw std::runtime_error("Not connected"); }

    // Copy data to ensure it stays alive during async operation
    auto data = std::make_shared<std::vector<std::byte>>(payload.begin(), payload.end());

    ws_->async_write(
      std::span<const std::byte>(*data), [data](const boost::system::error_code &error, std::size_t bytes_transferred) {
        if (error) {
          spdlog::error("[transport] Write failed: {} (attempted {} bytes)", error.message(), data->size());
        } else {
          spdlog::trace("[transport] Wrote {} bytes", bytes_transferred);
        }
        // data kept alive by capture until completion
      });
  }

  auto disconnect() -> void
  {
    if (connected_) {
      connected_ = false;
      ws_->async_close([](const boost::system::error_code & /*error*/, std::size_t /*bytes*/) {});
    }
  }
};

}// namespace radix_relay::nostr
