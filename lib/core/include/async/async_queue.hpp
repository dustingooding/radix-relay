#pragma once

#include <atomic>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstddef>
#include <memory>
#include <optional>

namespace radix_relay::async {

template<typename T> class async_queue
{
public:
  static const std::size_t channel_size{ 1024 };

  explicit async_queue(std::shared_ptr<boost::asio::io_context> io_context)// NOLINT(modernize-pass-by-value)
    : io_context_(io_context), channel_(*io_context_, channel_size),// NOLINT(performance-unnecessary-value-param)
      size_(0)
  {}

  auto push(T value) -> void
  {
    channel_.try_send(boost::system::error_code{}, std::move(value));
    ++size_;
  }

  auto pop() -> boost::asio::awaitable<T>
  {
    auto value = co_await channel_.async_receive(boost::asio::use_awaitable);
    --size_;
    co_return value;
  }

  // Non-blocking pop: returns std::nullopt if queue is empty
  // Use for draining queues or when you need to check without blocking.
  // For event-driven processing, prefer pop() in a coroutine loop.
  auto try_pop() -> std::optional<T>
  {
    T value;
    bool received =
      channel_.try_receive([&value](boost::system::error_code /*ec*/, T rx_value) { value = std::move(rx_value); });

    if (received) {
      --size_;
      return value;
    }
    return std::nullopt;
  }

  [[nodiscard]] auto empty() const -> bool { return size_.load() == 0; }

  [[nodiscard]] auto size() const -> std::size_t { return size_.load(); }

  auto cancel() -> void { channel_.cancel(); }

  auto close() -> void { channel_.close(); }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)> channel_;
  std::atomic<std::size_t> size_;
};

}// namespace radix_relay::async
