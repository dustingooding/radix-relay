#pragma once

#include <atomic>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstddef>
#include <memory>
#include <optional>

namespace radix_relay::async {

/**
 * @brief Thread-safe asynchronous queue for message passing between coroutines.
 *
 * @tparam T The type of elements stored in the queue
 *
 * Provides thread-safe push/pop operations using Boost.Asio concurrent channels.
 * Supports coroutine-based async pop operations with optional cancellation.
 */
template<typename T> class async_queue
{
public:
  /// Maximum number of elements the queue can hold
  static const std::size_t channel_size{ 1024 };

  /**
   * @brief Constructs a new async queue.
   *
   * @param io_context Shared pointer to the Boost.Asio io_context for async operations
   */
  explicit async_queue(const std::shared_ptr<boost::asio::io_context> &io_context)
    : io_context_(io_context), channel_(*io_context_, channel_size), size_(0)
  {}

  async_queue(const async_queue &) = delete;
  auto operator=(const async_queue &) -> async_queue & = delete;
  async_queue(async_queue &&) = delete;
  auto operator=(async_queue &&) -> async_queue & = delete;
  ~async_queue() = default;

  /**
   * @brief Pushes a value onto the queue (non-blocking).
   *
   * @param value The value to push (moved into the queue)
   */
  auto push(T value) -> void
  {
    channel_.try_send(boost::system::error_code{}, std::move(value));
    ++size_;
  }

  /**
   * @brief Asynchronously pops a value from the queue (coroutine).
   *
   * @param cancel_slot Optional cancellation slot for operation cancellation
   * @return Awaitable that yields the next value from the queue
   * @throws boost::system::system_error on cancellation or channel errors
   */
  auto pop(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<T>
  {
    boost::system::error_code err;

    if (cancel_slot) {
      auto val = co_await channel_.async_receive(boost::asio::bind_cancellation_slot(
        *cancel_slot, boost::asio::redirect_error(boost::asio::use_awaitable, err)));
      if (err) { throw boost::system::system_error(err); }
      --size_;
      co_return val;
    } else {
      auto val = co_await channel_.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, err));
      if (err) { throw boost::system::system_error(err); }
      --size_;
      co_return val;
    }
  }

  /**
   * @brief Attempts to pop a value without blocking.
   *
   * @return Optional containing the value if available, std::nullopt if queue is empty
   *
   * Use for draining queues or non-blocking checks. For event-driven processing, prefer pop().
   */
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

  /**
   * @brief Checks if the queue is empty.
   *
   * @return true if the queue contains no elements, false otherwise
   */
  [[nodiscard]] auto empty() const -> bool { return size_.load() == 0; }

  /**
   * @brief Returns the current number of elements in the queue.
   *
   * @return Current queue size
   */
  [[nodiscard]] auto size() const -> std::size_t { return size_.load(); }

  /**
   * @brief Closes the queue, preventing further operations.
   */
  auto close() -> void { channel_.close(); }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  boost::asio::experimental::concurrent_channel<void(boost::system::error_code, T)> channel_;
  std::atomic<std::size_t> size_;
};

}// namespace radix_relay::async
