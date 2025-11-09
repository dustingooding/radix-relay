#pragma once

#include <async/async_queue.hpp>
#include <concepts/websocket_stream.hpp>
#include <core/events.hpp>
#include <core/uuid_generator.hpp>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace radix_relay::nostr {

template<concepts::websocket_stream WebSocketStream> struct transport
{
  transport(std::shared_ptr<WebSocketStream> websocket_stream,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<boost::asio::io_context> io_context,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<async::async_queue<core::events::transport::in_t>> in_queue,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>>// NOLINT(modernize-pass-by-value)
      to_session_queue)
    : ws_(websocket_stream), io_context_(io_context), in_queue_(in_queue),// NOLINT(performance-unnecessary-value-param)
      to_session_queue_(to_session_queue)// NOLINT(performance-unnecessary-value-param)
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

  auto run_once() -> boost::asio::awaitable<void>
  {
    auto cmd = co_await in_queue_->pop();
    std::visit([&](auto &&event) { handle(std::forward<decltype(event)>(event)); }, cmd);
    co_return;
  }

  auto run() -> boost::asio::awaitable<void>
  {
    while (true) { co_await run_once(); }
    co_return;
  }

private:
  bool connected_{ false };
  std::shared_ptr<WebSocketStream> ws_;
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<core::events::transport::in_t>> in_queue_;
  std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> to_session_queue_;
  static constexpr size_t read_buffer_size = 8192;
  std::array<std::byte, read_buffer_size> read_buffer_{};
  std::unordered_map<std::string, std::vector<std::byte>> pending_sends_;

  std::string host_;
  std::string port_;
  std::string path_;

  auto emit_event(core::events::session_orchestrator::in_t evt) -> void { to_session_queue_->push(std::move(evt)); }

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
    } else if (not addr_str.starts_with("http")) {
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
        process_read(error, bytes_transferred);
      });
  }

  auto process_read(const boost::system::error_code &error, std::size_t bytes_transferred) -> void
  {
    if (not error and bytes_transferred > 0) {
      std::vector<std::byte> bytes(
        read_buffer_.begin(), read_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_transferred));

      core::events::transport::bytes_received evt{ .bytes = bytes };
      emit_event(std::move(evt));

      start_read();
    } else {
      connected_ = false;
    }
  }

  auto handle(const core::events::transport::connect &evt) noexcept -> void
  {
    try {
      parse_url(evt.url);
    } catch (const std::runtime_error &e) {
      core::events::transport::connect_failed failed{ .url = evt.url, .error_message = e.what() };
      emit_event(std::move(failed));
      return;
    }

    ws_->async_connect({ .host = host_, .port = port_, .path = path_ },
      [this, url = evt.url](const boost::system::error_code &error_code, std::size_t /*bytes*/) {
        if (not error_code) {
          connected_ = true;
          start_read();
          core::events::transport::connected connected_evt{ .url = url };
          emit_event(std::move(connected_evt));
        } else {
          core::events::transport::connect_failed failed{ .url = url, .error_message = error_code.message() };
          emit_event(std::move(failed));
        }
      });
  }

  auto handle(const core::events::transport::send &evt) noexcept -> void
  {
    if (not connected_) {
      core::events::transport::send_failed failed{ .message_id = evt.message_id, .error_message = "Not connected" };
      emit_event(std::move(failed));
      return;
    }

    std::shared_ptr<std::vector<std::byte>> data;
    try {
      data = std::make_shared<std::vector<std::byte>>(evt.bytes);
    } catch (const std::bad_alloc &e) {
      core::events::transport::send_failed failed{ .message_id = evt.message_id, .error_message = e.what() };
      emit_event(std::move(failed));
      return;
    }

    auto message_id = evt.message_id;

    ws_->async_write(std::span<const std::byte>(*data),
      [this, data, message_id](const boost::system::error_code &error, std::size_t bytes_transferred) {
        if (error) {
          spdlog::error("[transport] Write failed: {} (attempted {} bytes)", error.message(), data->size());
          core::events::transport::send_failed failed{ .message_id = message_id, .error_message = error.message() };
          emit_event(std::move(failed));
        } else {
          spdlog::trace("[transport] Wrote {} bytes", bytes_transferred);
          core::events::transport::sent sent_evt{ .message_id = message_id };
          emit_event(std::move(sent_evt));
        }
      });
  }

  auto handle(const core::events::transport::disconnect & /*evt*/) noexcept -> void
  {
    if (connected_) {
      connected_ = false;
      ws_->async_close([this](const boost::system::error_code & /*error*/, std::size_t /*bytes*/) {
        core::events::transport::disconnected evt{};
        emit_event(evt);
      });
    } else {
      core::events::transport::disconnected evt{};
      emit_event(evt);
    }
  }
};

}// namespace radix_relay::nostr
