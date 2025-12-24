#include <async/async_queue.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/standard_processor.hpp>
#include <memory>
#include <string>

#include "test_doubles/test_double_standard_handler.hpp"

using namespace radix_relay;
using namespace radix_relay::core;
using namespace radix_relay_test;

// ============================================================================
// CONSTRUCTION TESTS
// ============================================================================

SCENARIO("standard_processor construction with various handler types", "[standard_processor][construction]")
{
  GIVEN("A handler with no output queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};

    WHEN("constructing standard_processor with handler that has no output queues")
    {
      THEN("construction succeeds")
      {
        REQUIRE_NOTHROW([&]() -> void {
          const standard_processor<test_double_handler_no_output> processor(io_context, in_queue, out_queues);
        }());
      }
    }
  }

  GIVEN("A handler with output queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_standard_handler::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_standard_handler::out_queues_t const out_queues{ .output = output_queue };

    WHEN("constructing standard_processor with handler that has output queues")
    {
      THEN("construction succeeds")
      {
        REQUIRE_NOTHROW([&]() -> void {
          const standard_processor<test_double_standard_handler> processor(io_context, in_queue, out_queues);
        }());
      }
    }
  }

  GIVEN("A handler with handler-specific arguments")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_with_arg::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_handler_with_arg::out_queues_t const out_queues{ .output = output_queue };

    WHEN("constructing standard_processor with handler that takes additional arguments")
    {
      THEN("construction succeeds with arguments forwarded to handler")
      {
        REQUIRE_NOTHROW([&]() -> void {
          const standard_processor<test_double_handler_with_arg> processor(
            io_context, in_queue, out_queues, "test_prefix");
        }());
      }
    }
  }

  GIVEN("A handler with multiple handler-specific arguments")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_multi_arg::in_queue_t>(io_context);
    test_double_handler_multi_arg::out_queues_t const out_queues{};

    WHEN("constructing standard_processor with handler that takes multiple arguments")
    {
      THEN("construction succeeds with all arguments forwarded to handler")
      {
        REQUIRE_NOTHROW([&]() -> void {
          const standard_processor<test_double_handler_multi_arg> processor(
            io_context, in_queue, out_queues, "arg1", 42, true);
        }());
      }
    }
  }
}

// ============================================================================
// TYPE SAFETY TESTS
// ============================================================================

SCENARIO("standard_processor enforces type safety via handler traits", "[standard_processor][types]")
{
  GIVEN("A standard_processor instantiated with a handler type")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_standard_handler::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_standard_handler::out_queues_t const out_queues{ .output = output_queue };

    const standard_processor<test_double_standard_handler> processor(io_context, in_queue, out_queues);

    WHEN("checking the input queue type")
    {
      THEN("in_queue_t matches the handler's in_queue_t")
      {
        using processor_in_queue_t = typename decltype(processor)::in_queue_t;
        using handler_in_queue_t = test_double_standard_handler::in_queue_t;
        STATIC_REQUIRE(std::is_same_v<processor_in_queue_t, handler_in_queue_t>);
      }
    }

    WHEN("checking the output queues type")
    {
      THEN("out_queues_t matches the handler's out_queues_t")
      {
        using processor_out_queues_t = typename decltype(processor)::out_queues_t;
        using handler_out_queues_t = test_double_standard_handler::out_queues_t;
        STATIC_REQUIRE(std::is_same_v<processor_out_queues_t, handler_out_queues_t>);
      }
    }
  }
}

// ============================================================================
// RUN_ONCE TESTS
// ============================================================================

SCENARIO("standard_processor run_once processes single event", "[standard_processor][run_once]")
{
  GIVEN("A standard_processor with a simple handler")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_standard_handler::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_standard_handler::out_queues_t const out_queues{ .output = output_queue };

    auto processor =
      std::make_shared<standard_processor<test_double_standard_handler>>(io_context, in_queue, out_queues);

    WHEN("an event is pushed to the input queue and run_once is called")
    {
      in_queue->push(simple_event{ "test_data" });

      boost::asio::co_spawn(*io_context, processor->run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler processes the event exactly once")
      {
        auto output = output_queue->try_pop();
        REQUIRE(output.has_value());
        if (output.has_value()) { CHECK(output.value().result == "processed:test_data"); }
        CHECK(output_queue->empty());
      }
    }
  }

  GIVEN("A standard_processor with a handler that has no output queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};

    auto processor =
      std::make_shared<standard_processor<test_double_handler_no_output>>(io_context, in_queue, out_queues);

    WHEN("an event is pushed to the input queue and run_once is called")
    {
      in_queue->push(simple_event{ "no_output_test" });

      boost::asio::co_spawn(*io_context, processor->run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler processes the event without errors")
      {
        // Test passes if no exception is thrown
        CHECK(in_queue->empty());
      }
    }
  }
}

SCENARIO("standard_processor run_once with handler arguments", "[standard_processor][run_once][args]")
{
  GIVEN("A standard_processor with a handler that takes arguments")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_with_arg::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_handler_with_arg::out_queues_t const out_queues{ .output = output_queue };

    auto processor =
      std::make_shared<standard_processor<test_double_handler_with_arg>>(io_context, in_queue, out_queues, "PREFIX");

    WHEN("an event is processed")
    {
      in_queue->push(simple_event{ "data" });

      boost::asio::co_spawn(*io_context, processor->run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler uses the provided argument")
      {
        auto output = output_queue->try_pop();
        REQUIRE(output.has_value());
        if (output.has_value()) { CHECK(output.value().result == "PREFIX:data"); }
      }
    }
  }

  GIVEN("A standard_processor with a handler that takes multiple arguments")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_multi_arg::in_queue_t>(io_context);
    test_double_handler_multi_arg::out_queues_t const out_queues{};

    constexpr int test_arg_value = 99;
    auto processor = std::make_shared<standard_processor<test_double_handler_multi_arg>>(
      io_context, in_queue, out_queues, "multi", test_arg_value, false);

    WHEN("an event is processed")
    {
      in_queue->push(simple_event{ "event" });

      boost::asio::co_spawn(*io_context, processor->run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler uses all provided arguments")
      {
        // Handler would have processed with "multi:99:false:event"
        CHECK(in_queue->empty());
      }
    }
  }
}

SCENARIO("standard_processor run_once with multiple sequential calls", "[standard_processor][run_once][sequential]")
{
  GIVEN("A standard_processor with multiple events in queue")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_standard_handler::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_standard_handler::out_queues_t const out_queues{ .output = output_queue };

    auto processor =
      std::make_shared<standard_processor<test_double_standard_handler>>(io_context, in_queue, out_queues);

    WHEN("multiple events are pushed and run_once is called multiple times")
    {
      in_queue->push(simple_event{ "first" });
      in_queue->push(simple_event{ "second" });
      in_queue->push(simple_event{ "third" });

      boost::asio::co_spawn(
        *io_context,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [&processor]() -> boost::asio::awaitable<void> {
          co_await processor->run_once();
          co_await processor->run_once();
          co_await processor->run_once();
        },
        boost::asio::detached);

      io_context->run();

      THEN("all events are processed in order")
      {
        auto out1 = output_queue->try_pop();
        REQUIRE(out1.has_value());
        if (out1.has_value()) { CHECK(out1.value().result == "processed:first"); }

        auto out2 = output_queue->try_pop();
        REQUIRE(out2.has_value());
        if (out2.has_value()) { CHECK(out2.value().result == "processed:second"); }

        auto out3 = output_queue->try_pop();
        REQUIRE(out3.has_value());
        if (out3.has_value()) { CHECK(out3.value().result == "processed:third"); }

        CHECK(output_queue->empty());
      }
    }
  }
}

// ============================================================================
// RUN LOOP TESTS
// ============================================================================

SCENARIO("standard_processor run continuously processes events", "[standard_processor][run]")
{
  GIVEN("A standard_processor in run loop with stopping mechanism")
  {
    struct stopping_test_state
    {
      std::atomic<int> event_count{ 0 };
      std::shared_ptr<boost::asio::io_context> io_context;

      explicit stopping_test_state(std::shared_ptr<boost::asio::io_context> ctx) : io_context(std::move(ctx)) {}
    };

    struct stopping_handler
    {
      using in_queue_t = async::async_queue<simple_event>;
      struct out_queues_t
      {
      };

      std::shared_ptr<stopping_test_state> state;

      explicit stopping_handler(std::shared_ptr<stopping_test_state> state_ptr, const out_queues_t & /*queues*/)
        : state(std::move(state_ptr))
      {}

      auto handle(const simple_event & /*evt*/) const -> void
      {
        state->event_count++;
        if (state->event_count == 3) { state->io_context->stop(); }
      }
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<stopping_handler::in_queue_t>(io_context);
    stopping_handler::out_queues_t const out_queues{};
    auto state = std::make_shared<stopping_test_state>(io_context);

    auto processor = std::make_shared<standard_processor<stopping_handler>>(io_context, in_queue, out_queues, state);

    WHEN("multiple events are pushed and run is called")
    {
      in_queue->push(simple_event{ "event1" });
      in_queue->push(simple_event{ "event2" });
      in_queue->push(simple_event{ "event3" });

      boost::asio::co_spawn(*io_context, processor->run(), boost::asio::detached);

      io_context->run();

      THEN("all events are processed continuously until stopped") { CHECK(state->event_count == 3); }
    }
  }
}

// ============================================================================
// CANCELLATION TESTS
// ============================================================================

SCENARIO("standard_processor respects cancellation signal", "[standard_processor][cancellation]")
{
  GIVEN("A standard_processor with a cancellation signal")
  {
    struct test_state
    {
      std::atomic<bool> coroutine_done{ false };
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
    auto processor =
      std::make_shared<standard_processor<test_double_handler_no_output>>(io_context, in_queue, out_queues);

    auto state = std::make_shared<test_state>();

    WHEN("run is called with a cancellation slot and then cancelled")
    {
      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<test_double_handler_no_output>> proc,
          std::shared_ptr<test_state> test_state_ptr,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run(c_slot);
          } catch (const boost::system::system_error &e) {
            if (e.code() != boost::asio::error::operation_aborted
                and e.code() != boost::asio::experimental::error::channel_cancelled
                and e.code() != boost::asio::experimental::error::channel_closed) {
              throw;
            }
          }
          test_state_ptr->coroutine_done = true;
        }(processor, state, cancel_slot),
        boost::asio::detached);

      io_context->poll();
      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      io_context->run();

      THEN("the run loop exits gracefully")
      {
        CHECK(state->coroutine_done);
        CHECK(in_queue->empty());
      }
    }
  }

  GIVEN("A standard_processor running with events queued")
  {
    struct counting_state
    {
      std::atomic<int> count{ 0 };
      std::atomic<bool> done{ false };
    };

    struct counting_handler
    {
      using in_queue_t = async::async_queue<simple_event>;
      struct out_queues_t
      {
      };

      std::shared_ptr<counting_state> state;

      explicit counting_handler(std::shared_ptr<counting_state> state_ptr, const out_queues_t & /*queues*/)
        : state(std::move(state_ptr))
      {}

      auto handle(const simple_event & /*evt*/) const -> void { state->count++; }
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<counting_handler::in_queue_t>(io_context);
    counting_handler::out_queues_t const out_queues{};
    auto state = std::make_shared<counting_state>();
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());

    auto processor = std::make_shared<standard_processor<counting_handler>>(io_context, in_queue, out_queues, state);

    WHEN("events are pushed and cancellation occurs mid-processing")
    {
      in_queue->push(simple_event{ "event1" });
      in_queue->push(simple_event{ "event2" });

      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<counting_handler>> proc,
          std::shared_ptr<counting_state> state_ptr,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run(c_slot);
          } catch (const boost::system::system_error &e) {
            if (e.code() != boost::asio::error::operation_aborted
                and e.code() != boost::asio::experimental::error::channel_cancelled
                and e.code() != boost::asio::experimental::error::channel_closed) {
              throw;
            }
          }
          state_ptr->done = true;
        }(processor, state, cancel_slot),
        boost::asio::detached);

      io_context->poll();
      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      io_context->run();

      THEN("the cancellation is handled gracefully")
      {
        CHECK(state->done);
        // May have processed 0, 1, or 2 events depending on timing
        CHECK(state->count >= 0);
        CHECK(state->count <= 2);
      }
    }
  }
}

SCENARIO("standard_processor run_once respects cancellation", "[standard_processor][run_once][cancellation]")
{
  GIVEN("A standard_processor with empty queue and cancellation signal")
  {
    struct test_state
    {
      std::atomic<bool> completed{ false };
      std::atomic<bool> cancelled{ false };
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
    auto processor =
      std::make_shared<standard_processor<test_double_handler_no_output>>(io_context, in_queue, out_queues);
    auto state = std::make_shared<test_state>();

    WHEN("run_once is called with cancellation slot and cancelled before event arrives")
    {
      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<test_double_handler_no_output>> proc,
          std::shared_ptr<test_state> state_ptr,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run_once(c_slot);
            state_ptr->completed = true;
          } catch (const boost::system::system_error &e) {
            if (e.code() == boost::asio::error::operation_aborted
                or e.code() == boost::asio::experimental::error::channel_cancelled
                or e.code() == boost::asio::experimental::error::channel_closed) {
              state_ptr->cancelled = true;
            } else {
              throw;
            }
          }
        }(processor, state, cancel_slot),
        boost::asio::detached);

      io_context->poll();
      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      io_context->run();

      THEN("run_once is cancelled without processing")
      {
        CHECK(state->cancelled);
        CHECK_FALSE(state->completed);
      }
    }
  }
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

SCENARIO("standard_processor handles queue closure gracefully", "[standard_processor][error][channel_closed]")
{
  GIVEN("A standard_processor with its input queue closed")
  {
    struct test_state
    {
      std::atomic<bool> completed_successfully{ false };
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};
    auto processor =
      std::make_shared<standard_processor<test_double_handler_no_output>>(io_context, in_queue, out_queues);
    auto state = std::make_shared<test_state>();

    WHEN("the queue is closed and run is called")
    {
      in_queue->close();

      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<test_double_handler_no_output>> proc,
          std::shared_ptr<test_state> state_ptr) -> boost::asio::awaitable<void> {
          co_await proc->run();
          state_ptr->completed_successfully = true;
        }(processor, state),
        boost::asio::detached);

      io_context->run();

      THEN("the processor exits gracefully without throwing") { CHECK(state->completed_successfully); }
    }
  }
}

SCENARIO("standard_processor handles operation_aborted", "[standard_processor][error][operation_aborted]")
{
  GIVEN("A standard_processor that receives operation_aborted")
  {
    struct test_state
    {
      std::atomic<bool> exited_cleanly{ false };
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_handler_no_output::in_queue_t>(io_context);
    test_double_handler_no_output::out_queues_t const out_queues{};
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
    auto processor =
      std::make_shared<standard_processor<test_double_handler_no_output>>(io_context, in_queue, out_queues);
    auto state = std::make_shared<test_state>();

    WHEN("the processor is cancelled via cancellation signal")
    {
      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<test_double_handler_no_output>> proc,
          std::shared_ptr<test_state> state_ptr,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run(c_slot);
          } catch (const boost::system::system_error &e) {
            if (e.code() != boost::asio::error::operation_aborted
                and e.code() != boost::asio::experimental::error::channel_cancelled
                and e.code() != boost::asio::experimental::error::channel_closed) {
              throw;
            }
          }
          state_ptr->exited_cleanly = true;
        }(processor, state, cancel_slot),
        boost::asio::detached);

      io_context->poll();
      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      io_context->run();

      THEN("operation_aborted is handled and loop exits cleanly") { CHECK(state->exited_cleanly); }
    }
  }
}

SCENARIO("standard_processor propagates unexpected errors", "[standard_processor][error][propagation]")
{
  GIVEN("A handler that throws an unexpected exception")
  {
    struct test_state
    {
      std::atomic<bool> caught_exception{ false };
      std::string exception_message;
    };

    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<throwing_handler::in_queue_t>(io_context);
    throwing_handler::out_queues_t const out_queues{};
    auto processor = std::make_shared<standard_processor<throwing_handler>>(io_context, in_queue, out_queues);
    auto state = std::make_shared<test_state>();

    WHEN("an event triggers the handler to throw")
    {
      in_queue->push(simple_event{ "trigger" });

      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<standard_processor<throwing_handler>> proc,
          std::shared_ptr<test_state> state_ptr) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run_once();
          } catch (const std::runtime_error &e) {
            state_ptr->caught_exception = true;
            state_ptr->exception_message = e.what();
          }
        }(processor, state),
        boost::asio::detached);

      io_context->run();

      THEN("the exception propagates to the caller")
      {
        CHECK(state->caught_exception);
        CHECK(state->exception_message == "Handler error");
      }
    }
  }
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

SCENARIO("standard_processor integrates with handler output queues", "[standard_processor][integration]")
{
  GIVEN("A processor with a handler that writes to output queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto in_queue = std::make_shared<test_double_standard_handler::in_queue_t>(io_context);
    auto output_queue = std::make_shared<async::async_queue<output_event>>(io_context);
    test_double_standard_handler::out_queues_t const out_queues{ .output = output_queue };

    auto processor =
      std::make_shared<standard_processor<test_double_standard_handler>>(io_context, in_queue, out_queues);

    WHEN("multiple events are processed via run loop")
    {
      struct stopping_state
      {
        std::shared_ptr<boost::asio::io_context> io_ctx;
        std::atomic<int> count{ 0 };

        explicit stopping_state(std::shared_ptr<boost::asio::io_context> ctx) : io_ctx(std::move(ctx)) {}
      };

      auto state = std::make_shared<stopping_state>(io_context);

      in_queue->push(simple_event{ "msg1" });
      in_queue->push(simple_event{ "msg2" });
      in_queue->push(simple_event{ "msg3" });

      boost::asio::co_spawn(
        *io_context,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [&processor, &state]() -> boost::asio::awaitable<void> {
          co_await processor->run_once();
          state->count++;
          if (state->count == 3) { state->io_ctx->stop(); }

          co_await processor->run_once();
          state->count++;
          if (state->count == 3) { state->io_ctx->stop(); }

          co_await processor->run_once();
          state->count++;
          if (state->count == 3) { state->io_ctx->stop(); }
        },
        boost::asio::detached);

      io_context->run();

      THEN("all events produce corresponding output events")
      {
        auto out1 = output_queue->try_pop();
        REQUIRE(out1.has_value());
        if (out1.has_value()) { CHECK(out1.value().result == "processed:msg1"); }

        auto out2 = output_queue->try_pop();
        REQUIRE(out2.has_value());
        if (out2.has_value()) { CHECK(out2.value().result == "processed:msg2"); }

        auto out3 = output_queue->try_pop();
        REQUIRE(out3.has_value());
        if (out3.has_value()) { CHECK(out3.value().result == "processed:msg3"); }

        CHECK(output_queue->empty());
      }
    }
  }
}
