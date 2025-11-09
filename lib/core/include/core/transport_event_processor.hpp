#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <core/events.hpp>
#include <memory>
#include <variant>

namespace radix_relay::core {

template<typename TransportEventHandler> class transport_event_processor
{
public:
  transport_event_processor(std::shared_ptr<boost::asio::io_context> io_context,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<async::async_queue<events::transport_event_variant_t>> in_queue,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<TransportEventHandler> event_handler)// NOLINT(modernize-pass-by-value)
    : io_context_(io_context), in_queue_(in_queue),// NOLINT(performance-unnecessary-value-param)
      event_handler_(event_handler)// NOLINT(performance-unnecessary-value-param)
  {}

  auto run_once() -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop();
    std::visit([this](auto &&event) { event_handler_->handle(std::forward<decltype(event)>(event)); }, evt);
    co_return;
  }

  auto run() -> boost::asio::awaitable<void>
  {
    while (true) { co_await run_once(); }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::transport_event_variant_t>> in_queue_;
  std::shared_ptr<TransportEventHandler> event_handler_;
};

}// namespace radix_relay::core
