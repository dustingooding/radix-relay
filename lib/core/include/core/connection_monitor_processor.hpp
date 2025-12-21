#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <variant>

namespace radix_relay::core {

/**
 * @brief Processes connection monitor events (transport status and queries).
 */
class connection_monitor_processor
{
public:
  /**
   * @brief Constructs a connection monitor processor.
   *
   * @param io_context Boost.Asio io_context
   * @param in_queue Queue for incoming connection monitor events
   * @param monitor Connection monitor to update
   */
  connection_monitor_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> &in_queue,
    const std::shared_ptr<connection_monitor> &monitor)
    : io_context_(io_context), in_queue_(in_queue), monitor_(monitor)
  {}

  /**
   * @brief Processes a single event from the queue.
   *
   * @param cancel_slot Optional cancellation slot
   * @return Awaitable that completes after processing one event
   */
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop(cancel_slot);
    std::visit([&](auto &&event) { monitor_->handle(std::forward<decltype(event)>(event)); }, evt);
    co_return;
  }

  /**
   * @brief Continuously processes events until cancelled.
   *
   * @param cancel_slot Optional cancellation slot
   * @return Awaitable that runs until cancellation or error
   */
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    try {
      while (true) { co_await run_once(cancel_slot); }
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted
          or e.code() == boost::asio::experimental::error::channel_cancelled
          or e.code() == boost::asio::experimental::error::channel_closed) {
        spdlog::debug("[connection_monitor_processor] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[connection_monitor_processor] Unexpected error in run loop: {}", e.what());
        throw;
      }
    }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> in_queue_;
  std::shared_ptr<connection_monitor> monitor_;
};

}// namespace radix_relay::core
