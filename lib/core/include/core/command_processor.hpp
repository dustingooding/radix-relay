#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <core/events.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace radix_relay::core {

template<typename EventHandler> class command_processor
{
public:
  command_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<events::raw_command>> &in_queue,
    const std::shared_ptr<EventHandler> &event_handler)
    : io_context_(io_context), in_queue_(in_queue), event_handler_(event_handler)
  {}

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto cmd = co_await in_queue_->pop(cancel_slot);
    event_handler_->handle(cmd);
    co_return;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    try {
      while (true) { co_await run_once(cancel_slot); }
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted
          or e.code() == boost::asio::experimental::error::channel_cancelled
          or e.code() == boost::asio::experimental::error::channel_closed) {
        spdlog::debug("[command_processor] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[command_processor] Unexpected error in run loop: {}", e.what());
        throw;
      }
    }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::raw_command>> in_queue_;
  std::shared_ptr<EventHandler> event_handler_;
};

}// namespace radix_relay::core
