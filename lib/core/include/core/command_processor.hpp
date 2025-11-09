#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <core/events.hpp>
#include <memory>

namespace radix_relay::core {

template<typename EventHandler> class command_processor
{
public:
  command_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<events::raw_command>> &in_queue,
    const std::shared_ptr<EventHandler> &event_handler)
    : io_context_(io_context), in_queue_(in_queue), event_handler_(event_handler)
  {}

  auto run_once() -> boost::asio::awaitable<void>
  {
    auto cmd = co_await in_queue_->pop();
    event_handler_->handle(cmd);
    co_return;
  }

  auto run() -> boost::asio::awaitable<void>
  {
    while (true) { co_await run_once(); }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::raw_command>> in_queue_;
  std::shared_ptr<EventHandler> event_handler_;
};

}// namespace radix_relay::core
