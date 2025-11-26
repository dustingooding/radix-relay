#pragma once

#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <core/events.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <variant>

namespace radix_relay::core {

/**
 * @brief Processes presentation events from a queue using a presentation handler.
 *
 * @tparam PresentationEventHandler Type that handles presentation events
 *
 * Continuously pulls presentation events from a queue and dispatches them to the handler.
 */
template<typename PresentationEventHandler> class presentation_processor
{
public:
  /**
   * @brief Constructs a presentation processor.
   *
   * @param io_context Boost.Asio io_context for async operations
   * @param in_queue Queue containing incoming presentation events
   * @param event_handler Handler to process presentation events
   */
  presentation_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<events::presentation_event_variant_t>> &in_queue,
    const std::shared_ptr<PresentationEventHandler> &event_handler)
    : io_context_(io_context), in_queue_(in_queue), event_handler_(event_handler)
  {}

  /**
   * @brief Processes a single presentation event from the queue.
   *
   * @param cancel_slot Optional cancellation slot
   * @return Awaitable that completes after processing one event
   */
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop(cancel_slot);
    std::visit([this](auto &&event) { event_handler_->handle(std::forward<decltype(event)>(event)); }, evt);
    co_return;
  }

  /**
   * @brief Continuously processes presentation events until cancelled.
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
        spdlog::debug("[presentation_processor] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[presentation_processor] Unexpected error in run loop: {}", e.what());
        throw;
      }
    }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<events::presentation_event_variant_t>> in_queue_;
  std::shared_ptr<PresentationEventHandler> event_handler_;
};

}// namespace radix_relay::core
