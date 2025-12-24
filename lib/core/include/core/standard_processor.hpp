#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace radix_relay::core {

/**
 * @brief Generic queue-processing loop that HasA handler (no virtual inheritance).
 *
 * @tparam Handler Handler type that provides in_queue_t and out_queues_t type traits
 *
 * Standard processor eliminates code duplication across different processor types by
 * providing a single implementation of the async run loop with error handling.
 * The handler specifies its queue dependencies via type traits:
 * - in_queue_t: The type of async_queue the processor reads from
 * - out_queues_t: A struct with named members for output queues
 */
template<typename Handler> class standard_processor
{
public:
  using in_queue_t = typename Handler::in_queue_t;
  using out_queues_t = typename Handler::out_queues_t;

  /**
   * @brief Constructs a standard processor with handler-specific arguments.
   *
   * @tparam HandlerArgs Types of handler-specific constructor arguments
   * @param io_context Boost.Asio io_context for async operations
   * @param in_queue Input queue to read events from
   * @param out_queues Output queues struct (passed to handler constructor)
   * @param handler_args Handler-specific arguments forwarded to handler constructor
   *
   * The constructor creates the handler by forwarding handler_args followed by out_queues
   * to the Handler constructor. This supports handlers with varying constructor signatures.
   */
  template<typename... HandlerArgs>
  standard_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<in_queue_t> &in_queue,
    const out_queues_t &out_queues,
    HandlerArgs &&...handler_args)
    : io_context_(io_context), in_queue_(in_queue),
      handler_(std::make_shared<Handler>(std::forward<HandlerArgs>(handler_args)..., out_queues))
  {}

  /**
   * @brief Processes a single event from the input queue.
   *
   * @param cancel_slot Optional cancellation slot for cancelling the async operation
   * @return Awaitable that completes after processing one event
   *
   * Pops one event from the input queue, passes it to the handler's handle() method,
   * and returns. This is useful for controlled event processing and testing.
   */
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop(cancel_slot);
    handler_->handle(evt);
    co_return;
  }

  /**
   * @brief Continuously processes events from the input queue until cancelled or closed.
   *
   * @param cancel_slot Optional cancellation slot for cancelling the async operation
   * @return Awaitable that runs until cancellation or queue closure
   *
   * Implements the standard async run loop with error handling. Continuously pops events
   * and dispatches them to the handler. Handles expected errors (cancellation, channel
   * closure) gracefully while propagating unexpected errors.
   */
  auto run(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    try {
      while (true) { co_await run_once(cancel_slot); }
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::operation_aborted
          or e.code() == boost::asio::experimental::error::channel_cancelled
          or e.code() == boost::asio::experimental::error::channel_closed) {
        spdlog::debug("[standard_processor] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[standard_processor] Unexpected error in run loop: {}", e.what());
        throw;
      }
    }
  }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<in_queue_t> in_queue_;
  std::shared_ptr<Handler> handler_;
};

}// namespace radix_relay::core
