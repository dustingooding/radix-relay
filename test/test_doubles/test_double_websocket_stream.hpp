#pragma once

#include <concepts/websocket_stream.hpp>
#include <transport/websocket_stream.hpp>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace radix_relay::test {

class test_double_websocket_stream
{
public:
  struct connection_record
  {
    std::string host;
    std::string port;
    std::string path;
  };

  struct write_record
  {
    std::vector<std::byte> data;
  };

  explicit test_double_websocket_stream(const std::shared_ptr<boost::asio::io_context> &io_context)
    : io_context_(io_context)
  {}

  auto set_connect_failure(bool fail) -> void { should_fail_connect_ = fail; }
  auto set_write_failure(bool fail) -> void { should_fail_write_ = fail; }
  auto set_read_failure(bool fail) -> void { should_fail_read_ = fail; }
  auto set_close_failure(bool fail) -> void { should_fail_close_ = fail; }

  auto set_read_data(std::vector<std::byte> data) -> void
  {
    read_data_ = std::move(data);
    read_position_ = 0;

    if (pending_read_handler_) { complete_pending_read(); }
  }

  [[nodiscard]] auto get_connections() const -> const std::vector<connection_record> & { return connections_; }
  [[nodiscard]] auto get_writes() const -> const std::vector<write_record> & { return writes_; }
  [[nodiscard]] auto is_connected() const -> bool { return connected_; }

  auto reset() -> void
  {
    connections_.clear();
    writes_.clear();
    read_data_.clear();
    read_position_ = 0;
    connected_ = false;
    should_fail_connect_ = false;
    should_fail_write_ = false;
    should_fail_read_ = false;
    should_fail_close_ = false;
  }

  auto async_connect(radix_relay::transport::websocket_connection_params params,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
  {
    connections_.push_back({ std::string(params.host), std::string(params.port), std::string(params.path) });

    boost::asio::post(*io_context_, [this, handler = std::move(handler)]() {
      if (should_fail_connect_) {
        handler(boost::asio::error::connection_refused, 0);
      } else {
        connected_ = true;
        handler(boost::system::error_code{}, 0);
      }
    });
  }

  auto async_write(std::span<const std::byte> data,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
  {
    writes_.push_back({ std::vector<std::byte>(data.begin(), data.end()) });

    const auto bytes = data.size();
    boost::asio::post(*io_context_, [this, bytes, handler = std::move(handler)]() {
      if (should_fail_write_) {
        handler(boost::asio::error::broken_pipe, 0);
      } else {
        handler(boost::system::error_code{}, bytes);
      }
    });
  }

  auto async_read(const boost::asio::mutable_buffer &buffer,
    std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
  {
    if (should_fail_read_) {
      boost::asio::post(
        *io_context_, [handler = std::move(handler)]() { handler(boost::asio::error::connection_reset, 0); });
      return;
    }

    pending_read_handler_ = std::move(handler);
    pending_read_buffer_ = buffer;

    if (read_position_ < read_data_.size()) { complete_pending_read(); }
  }

  auto async_close(std::function<void(const boost::system::error_code &, std::size_t)> handler) -> void
  {
    boost::asio::post(*io_context_, [this, handler = std::move(handler)]() {
      if (should_fail_close_) {
        handler(boost::asio::error::operation_aborted, 0);
      } else {
        connected_ = false;
        handler(boost::system::error_code{}, 0);
      }
    });
  }

private:
  auto complete_pending_read() -> void
  {
    if (not pending_read_handler_) { return; }

    const auto buffer_size = pending_read_buffer_.size();
    const auto available = read_data_.size() - read_position_;
    const auto to_read = std::min(buffer_size, available);

    if (to_read > 0) {
      std::copy_n(read_data_.begin() + static_cast<std::ptrdiff_t>(read_position_),
        to_read,
        static_cast<std::byte *>(pending_read_buffer_.data()));
      read_position_ += to_read;
    }

    auto handler = std::move(pending_read_handler_);
    pending_read_handler_ = nullptr;

    boost::asio::post(
      *io_context_, [handler = std::move(handler), to_read]() { handler(boost::system::error_code{}, to_read); });
  }

  std::shared_ptr<boost::asio::io_context> io_context_;

  bool should_fail_connect_{ false };
  bool should_fail_write_{ false };
  bool should_fail_read_{ false };
  bool should_fail_close_{ false };

  std::vector<connection_record> connections_;
  std::vector<write_record> writes_;
  std::vector<std::byte> read_data_;
  size_t read_position_{ 0 };
  bool connected_{ false };

  std::function<void(const boost::system::error_code &, std::size_t)> pending_read_handler_;
  boost::asio::mutable_buffer pending_read_buffer_;
};

static_assert(radix_relay::concepts::websocket_stream<test_double_websocket_stream>);

}// namespace radix_relay::test
