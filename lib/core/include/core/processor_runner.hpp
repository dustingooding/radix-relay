#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <concepts>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace radix_relay::core {

template<typename T>
concept Processor = requires(T proc, std::shared_ptr<boost::asio::cancellation_slot> slot) {
  { proc.run(slot) } -> std::same_as<boost::asio::awaitable<void>>;
};

template<Processor P>
auto run_processor(std::shared_ptr<P> proc,
  std::shared_ptr<boost::asio::cancellation_slot> cancel_slot,
  std::string_view processor_name) -> boost::asio::awaitable<void>
{
  spdlog::trace("[{}] Coroutine started", processor_name);
  try {
    co_await proc->run(cancel_slot);
  } catch (const boost::system::system_error &err) {
    if (err.code() == boost::asio::error::operation_aborted
        or err.code() == boost::asio::experimental::error::channel_cancelled
        or err.code() == boost::asio::experimental::error::channel_closed) {
      spdlog::debug("[{}] Cancelled, exiting run loop", processor_name);
      co_return;
    }
    spdlog::error("[{}] Unexpected error in run_once: {}", processor_name, err.what());
  } catch (const std::exception &err) {
    spdlog::error("[{}] Unknown exception in run_once: {}", processor_name, err.what());
  }
  spdlog::trace("[{}] Coroutine exiting", processor_name);
}

struct coroutine_state
{
  std::atomic<bool> started{ false };
  std::atomic<bool> done{ false };
};

template<Processor P>
auto spawn_processor(const std::shared_ptr<boost::asio::io_context> &io_ctx,
  std::shared_ptr<P> proc,
  std::shared_ptr<boost::asio::cancellation_slot> cancel_slot,
  std::string_view processor_name) -> std::shared_ptr<coroutine_state>
{
  auto state = std::make_shared<coroutine_state>();
  boost::asio::co_spawn(
    *io_ctx,
    [](std::shared_ptr<P> processor,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot,
      std::string_view name,
      std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
      coro_state->started = true;
      co_await run_processor(processor, c_slot, name);
      coro_state->done = true;
    }(proc, cancel_slot, processor_name, state),
    boost::asio::detached);
  return state;
}

}// namespace radix_relay::core
