#pragma once

#include <radix_relay/concepts/websocket_stream.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/transport/uuid_generator.hpp>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace radix_relay::nostr {

template<concepts::websocket_stream WebSocketStream> class transport
{
public:
  using send_event_fn_t = std::function<void(core::events::transport::event_variant_t)>;

private:
  bool connected_{ false };
  send_event_fn_t send_event_;
  const boost::asio::io_context::strand *session_strand_;
  std::shared_ptr<WebSocketStream> ws_;
  boost::asio::io_context &io_context_;
  static constexpr size_t read_buffer_size = 8192;
  std::array<std::byte, read_buffer_size> read_buffer_{};
  std::unordered_map<std::string, std::vector<std::byte>> pending_sends_;

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
      std::vector<std::byte> bytes(
        read_buffer_.begin(), read_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_transferred));

      boost::asio::post(*session_strand_, [this, bytes]() {
        core::events::transport::bytes_received evt{ .bytes = bytes };
        send_event_(std::move(evt));
      });

      start_read();
    } else {
      connected_ = false;
    }
  }

public:
  transport(std::shared_ptr<WebSocketStream> websocket_stream,
    boost::asio::io_context &io_context,
    const boost::asio::io_context::strand *session_strand,
    send_event_fn_t send_event)
    : send_event_(std::move(send_event)), session_strand_(session_strand), ws_(std::move(websocket_stream)),
      io_context_(io_context)
  {}

  ~transport()
  {
    if (connected_) {
      connected_ = false;
      ws_->async_close([](const boost::system::error_code & /*error*/, std::size_t /*bytes*/) {});
    }
  }

  transport(const transport &) = delete;
  auto operator=(const transport &) -> transport & = delete;
  transport(transport &&) = delete;
  auto operator=(transport &&) -> transport & = delete;

  auto handle_command(const core::events::transport::connect &cmd) noexcept -> void
  {
    // Parse URL - can throw std::runtime_error for invalid URLs
    try {
      parse_url(cmd.url);
    } catch (const std::runtime_error &e) {
      boost::asio::post(*session_strand_, [this, url = cmd.url, error_msg = std::string(e.what())]() {
        core::events::transport::connect_failed evt{ .url = url, .error_message = error_msg };
        send_event_(std::move(evt));
      });
      return;
    }

    // Initiate async connect
    ws_->async_connect({ .host = host_, .port = port_, .path = path_ },
      [this, url = cmd.url](const boost::system::error_code &error_code, std::size_t /*bytes*/) {
        if (!error_code) {
          connected_ = true;
          start_read();
          boost::asio::post(*session_strand_, [this, url]() {
            core::events::transport::connected evt{ .url = url };
            send_event_(std::move(evt));
          });
        } else {
          boost::asio::post(*session_strand_, [this, url, error_msg = error_code.message()]() {
            core::events::transport::connect_failed evt{ .url = url, .error_message = error_msg };
            send_event_(std::move(evt));
          });
        }
      });
  }

  auto handle_command(const core::events::transport::send &cmd) noexcept -> void
  {
    if (!connected_) {
      boost::asio::post(*session_strand_, [this, message_id = cmd.message_id]() {
        core::events::transport::send_failed evt{ .message_id = message_id, .error_message = "Not connected" };
        send_event_(std::move(evt));
      });
      return;
    }

    // Allocate data buffer - can throw std::bad_alloc
    std::shared_ptr<std::vector<std::byte>> data;
    try {
      data = std::make_shared<std::vector<std::byte>>(cmd.bytes);
    } catch (const std::bad_alloc &e) {
      boost::asio::post(*session_strand_, [this, message_id = cmd.message_id, error_msg = std::string(e.what())]() {
        core::events::transport::send_failed evt{ .message_id = message_id, .error_message = error_msg };
        send_event_(std::move(evt));
      });
      return;
    }

    auto message_id = cmd.message_id;

    ws_->async_write(std::span<const std::byte>(*data),
      [this, data, message_id](const boost::system::error_code &error, std::size_t bytes_transferred) {
        if (error) {
          spdlog::error("[transport] Write failed: {} (attempted {} bytes)", error.message(), data->size());
          boost::asio::post(*session_strand_, [this, message_id, error_msg = error.message()]() {
            core::events::transport::send_failed evt{ .message_id = message_id, .error_message = error_msg };
            send_event_(std::move(evt));
          });
        } else {
          spdlog::trace("[transport] Wrote {} bytes", bytes_transferred);
          boost::asio::post(*session_strand_, [this, message_id]() {
            core::events::transport::sent evt{ .message_id = message_id };
            send_event_(std::move(evt));
          });
        }
      });
  }

  auto handle_command(const core::events::transport::disconnect & /*cmd*/) noexcept -> void
  {
    if (connected_) {
      connected_ = false;
      ws_->async_close([this](const boost::system::error_code & /*error*/, std::size_t /*bytes*/) {
        boost::asio::post(*session_strand_, [this]() {
          core::events::transport::disconnected evt{};
          send_event_(evt);
        });
      });
    } else {
      boost::asio::post(*session_strand_, [this]() {
        core::events::transport::disconnected evt{};
        send_event_(evt);
      });
    }
  }
};

}// namespace radix_relay::nostr
