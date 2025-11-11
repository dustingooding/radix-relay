#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

using namespace radix_relay;

struct test_event
{
  std::string data;
};

template<typename EventType> class test_processor
{
public:
  test_processor(const std::shared_ptr<boost::asio::io_context> &io_context,
    const std::shared_ptr<async::async_queue<EventType>> &in_queue)
    : io_context_(io_context), in_queue_(in_queue), processed_count_(std::make_shared<std::size_t>(0))
  {}

  test_processor(const test_processor &) = delete;
  auto operator=(const test_processor &) -> test_processor & = delete;
  test_processor(test_processor &&) = delete;
  auto operator=(test_processor &&) -> test_processor & = delete;

  ~test_processor()
  {
    spdlog::debug("[test_processor] Destructor called, processed {} events", *processed_count_);
    in_queue_->close();
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run_once(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    auto evt = co_await in_queue_->pop(cancel_slot);
    ++(*processed_count_);
    spdlog::debug("[test_processor] Processed event: {}", evt.data);
    co_return;
  }

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  auto run(std::shared_ptr<boost::asio::cancellation_slot> cancel_slot = nullptr) -> boost::asio::awaitable<void>
  {
    try {
      while (true) { co_await run_once(cancel_slot); }
    } catch (const boost::system::system_error &err) {
      if (err.code() == boost::asio::error::operation_aborted
          or err.code() == boost::asio::experimental::error::channel_cancelled
          or err.code() == boost::asio::experimental::error::channel_closed) {
        spdlog::debug("[test_processor] Cancelled, exiting run loop");
        co_return;
      } else {
        spdlog::error("[test_processor] Unexpected error in run loop: {}", err.what());
        throw;
      }
    }
  }

  [[nodiscard]] auto processed_count() const -> std::size_t { return *processed_count_; }

private:
  std::shared_ptr<boost::asio::io_context> io_context_;
  std::shared_ptr<async::async_queue<EventType>> in_queue_;
  std::shared_ptr<std::size_t> processed_count_;
};

TEST_CASE("coroutine lifecycle - basic cleanup with cancellation", "[coroutine][lifecycle]")
{
  spdlog::set_level(spdlog::level::debug);

  constexpr auto event_processing_delay_ms = 50;
  constexpr auto coroutine_check_interval_ms = 10;

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
  auto queue = std::make_shared<async::async_queue<test_event>>(io_context);
  auto processor = std::make_shared<test_processor<test_event>>(io_context, queue);

  struct coroutine_state
  {
    std::atomic<bool> started{ false };
    std::atomic<bool> done{ false };
  };
  auto state = std::make_shared<coroutine_state>();

  queue->push(test_event{ "event1" });
  queue->push(test_event{ "event2" });

  boost::asio::co_spawn(
    *io_context,
    [](std::shared_ptr<test_processor<test_event>> proc,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot,
      std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
      coro_state->started = true;
      spdlog::debug("[test] Coroutine started");
      try {
        co_await proc->run(c_slot);
      } catch (const boost::system::system_error &err) {
        if (err.code() != boost::asio::error::operation_aborted
            and err.code() != boost::asio::experimental::error::channel_cancelled
            and err.code() != boost::asio::experimental::error::channel_closed) {
          spdlog::error("[test] Unexpected error: {}", err.what());
        }
      }
      spdlog::debug("[test] Coroutine exiting");
      coro_state->done = true;
    }(processor, cancel_slot, state),
    boost::asio::detached);

  std::thread io_thread([&io_context]() { io_context->run(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(event_processing_delay_ms));

  spdlog::debug("[test] Emitting cancellation signal");
  cancel_signal->emit(boost::asio::cancellation_type::terminal);
  queue->close();

  constexpr auto timeout = std::chrono::seconds(2);
  auto start = std::chrono::steady_clock::now();
  while (state->started != state->done) {
    std::this_thread::sleep_for(std::chrono::milliseconds(coroutine_check_interval_ms));
    if (std::chrono::steady_clock::now() - start > timeout) {
      FAIL("Timeout waiting for coroutine to complete");
      break;
    }
  }

  spdlog::debug("[test] Stopping io_context");
  io_context->stop();

  if (io_thread.joinable()) { io_thread.join(); }

  spdlog::debug("[test] Processed count: {}", processor->processed_count());
  CHECK(processor->processed_count() == 2);
  CHECK(state->done);
}

TEST_CASE("coroutine lifecycle - immediate cancellation", "[coroutine][lifecycle]")
{
  spdlog::set_level(spdlog::level::debug);

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
  auto queue = std::make_shared<async::async_queue<test_event>>(io_context);
  auto processor = std::make_shared<test_processor<test_event>>(io_context, queue);

  struct coroutine_state
  {
    std::atomic<bool> started{ false };
    std::atomic<bool> done{ false };
  };
  auto state = std::make_shared<coroutine_state>();

  boost::asio::co_spawn(
    *io_context,
    [](std::shared_ptr<test_processor<test_event>> proc,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot,
      std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
      coro_state->started = true;
      spdlog::debug("[test] Coroutine started");
      try {
        co_await proc->run(c_slot);
      } catch (const boost::system::system_error &err) {
        if (err.code() != boost::asio::error::operation_aborted
            and err.code() != boost::asio::experimental::error::channel_cancelled
            and err.code() != boost::asio::experimental::error::channel_closed) {
          spdlog::error("[test] Unexpected error: {}", err.what());
        }
      }
      spdlog::debug("[test] Coroutine exiting");
      coro_state->done = true;
    }(processor, cancel_slot, state),
    boost::asio::detached);

  io_context->poll();

  spdlog::debug("[test] Emitting cancellation signal immediately");
  cancel_signal->emit(boost::asio::cancellation_type::terminal);
  queue->close();

  io_context->run();

  CHECK(processor->processed_count() == 0);
  CHECK(state->done);
}

TEST_CASE("coroutine lifecycle - RAII with scope exit", "[coroutine][lifecycle][raii]")
{
  spdlog::set_level(spdlog::level::debug);

  constexpr auto event_processing_delay_ms = 50;
  constexpr auto coroutine_check_interval_ms = 10;

  std::size_t final_processed_count = 0;

  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());

    {
      auto queue = std::make_shared<async::async_queue<test_event>>(io_context);
      auto processor = std::make_shared<test_processor<test_event>>(io_context, queue);

      struct coroutine_state
      {
        std::atomic<bool> started{ false };
        std::atomic<bool> done{ false };
      };
      auto state = std::make_shared<coroutine_state>();

      queue->push(test_event{ "event1" });
      queue->push(test_event{ "event2" });
      queue->push(test_event{ "event3" });

      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<test_processor<test_event>> proc,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot,
          std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
          coro_state->started = true;
          spdlog::debug("[test] Coroutine started");
          try {
            co_await proc->run(c_slot);
          } catch (const boost::system::system_error &err) {
            if (err.code() != boost::asio::error::operation_aborted
                and err.code() != boost::asio::experimental::error::channel_cancelled) {
              spdlog::error("[test] Unexpected error: {}", err.what());
            }
          }
          spdlog::debug("[test] Coroutine exiting");
          coro_state->done = true;
        }(processor, cancel_slot, state),
        boost::asio::detached);

      std::thread io_thread([&io_context]() { io_context->run(); });

      std::this_thread::sleep_for(std::chrono::milliseconds(event_processing_delay_ms));

      spdlog::debug("[test] Emitting cancellation signal");
      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      queue->close();

      constexpr auto timeout = std::chrono::seconds(2);
      auto start = std::chrono::steady_clock::now();
      while (state->started != state->done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(coroutine_check_interval_ms));
        if (std::chrono::steady_clock::now() - start > timeout) {
          FAIL("Timeout waiting for coroutine to complete");
          break;
        }
      }

      io_context->stop();

      if (io_thread.joinable()) { io_thread.join(); }

      final_processed_count = processor->processed_count();

      spdlog::debug("[test] Leaving inner scope - processor and queue should destruct");
    }

    spdlog::debug("[test] Leaving outer scope - io_context and cancel_signal should destruct");
  }

  spdlog::debug("[test] All scopes exited, checking results");
  CHECK(final_processed_count == 3);
}

TEST_CASE("coroutine lifecycle - multiple processors with shared cancellation", "[coroutine][lifecycle][multi]")
{
  spdlog::set_level(spdlog::level::debug);

  constexpr auto event_processing_delay_ms = 50;

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());

  std::size_t final_processor1_count = 0;
  std::size_t final_processor2_count = 0;

  struct coroutine_state
  {
    std::atomic<bool> started{ false };
    std::atomic<bool> done{ false };
  };

  auto state1 = std::make_shared<coroutine_state>();
  auto state2 = std::make_shared<coroutine_state>();

  auto queue1 = std::make_shared<async::async_queue<test_event>>(io_context);
  auto queue2 = std::make_shared<async::async_queue<test_event>>(io_context);

  {
    auto processor1 = std::make_shared<test_processor<test_event>>(io_context, queue1);
    auto processor2 = std::make_shared<test_processor<test_event>>(io_context, queue2);

    queue1->push(test_event{ "p1-event1" });
    queue1->push(test_event{ "p1-event2" });

    queue2->push(test_event{ "p2-event1" });
    queue2->push(test_event{ "p2-event2" });
    queue2->push(test_event{ "p2-event3" });

    auto future1 = boost::asio::co_spawn(
      *io_context,
      [](std::shared_ptr<test_processor<test_event>> proc,
        std::shared_ptr<boost::asio::cancellation_slot> c_slot,
        std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
        coro_state->started = true;
        spdlog::debug("[test] Processor1 coroutine started");
        try {
          co_await proc->run(c_slot);
        } catch (const boost::system::system_error &err) {
          if (err.code() != boost::asio::error::operation_aborted
              and err.code() != boost::asio::experimental::error::channel_cancelled
              and err.code() != boost::asio::experimental::error::channel_closed) {
            spdlog::error("[test] Processor1 unexpected error: {}", err.what());
          }
        }
        spdlog::debug("[test] Processor1 coroutine exiting");
        coro_state->done = true;
      }(processor1, cancel_slot, state1),
      boost::asio::use_future);

    auto future2 = boost::asio::co_spawn(
      *io_context,
      [](std::shared_ptr<test_processor<test_event>> proc,
        std::shared_ptr<boost::asio::cancellation_slot> c_slot,
        std::shared_ptr<coroutine_state> coro_state) -> boost::asio::awaitable<void> {
        coro_state->started = true;
        spdlog::debug("[test] Processor2 coroutine started");
        try {
          co_await proc->run(c_slot);
        } catch (const boost::system::system_error &err) {
          if (err.code() != boost::asio::error::operation_aborted
              and err.code() != boost::asio::experimental::error::channel_cancelled
              and err.code() != boost::asio::experimental::error::channel_closed) {
            spdlog::error("[test] Processor2 unexpected error: {}", err.what());
          }
        }
        spdlog::debug("[test] Processor2 coroutine exiting");
        coro_state->done = true;
      }(processor2, cancel_slot, state2),
      boost::asio::use_future);

    std::thread io_thread([&io_context]() { io_context->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(event_processing_delay_ms));

    boost::asio::post(*io_context, [cancel_signal]() {
      spdlog::debug("[test] posting emit() to io_context thread");
      cancel_signal->emit(boost::asio::cancellation_type::all);
    });

    queue1->close();
    queue2->close();

    spdlog::debug("[test] Waiting for coroutines via futures");
    auto status1 = future1.wait_for(std::chrono::seconds(2));
    auto status2 = future2.wait_for(std::chrono::seconds(2));

    if (status1 != std::future_status::ready or status2 != std::future_status::ready) {
      spdlog::error("[test] Timeout waiting for futures - status1: {}, status2: {}",
        static_cast<int>(status1),
        static_cast<int>(status2));
      FAIL("Timeout waiting for coroutines to complete");
    }

    spdlog::debug("[test] Stopping io_context");
    io_context->stop();

    if (io_thread.joinable()) { io_thread.join(); }

    final_processor1_count = processor1->processed_count();
    final_processor2_count = processor2->processed_count();
  }

  spdlog::debug("[test] Processor1 count: {}, Processor2 count: {}", final_processor1_count, final_processor2_count);
  CHECK(final_processor1_count == 2);
  CHECK(final_processor2_count == 3);
  CHECK(state1->done);
  CHECK(state2->done);
}
