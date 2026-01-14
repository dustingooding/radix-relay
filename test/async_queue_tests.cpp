#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <async/async_queue.hpp>

TEST_CASE("async_queue can be constructed with int type", "[async_queue][construction]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  const radix_relay::async::async_queue<int> queue(io_context);

  CHECK(queue.empty());
}

TEST_CASE("async_queue can be constructed with string type", "[async_queue][construction]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  const radix_relay::async::async_queue<std::string> queue(io_context);

  CHECK(queue.empty());
}

TEST_CASE("async_queue push single value", "[async_queue][push]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  constexpr int test_value = 42;
  queue.push(test_value);

  CHECK_FALSE(queue.empty());
  REQUIRE(queue.size() == 1);
}

TEST_CASE("async_queue push multiple values", "[async_queue][push]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  queue.push(1);
  queue.push(2);
  queue.push(3);

  CHECK_FALSE(queue.empty());
  REQUIRE(queue.size() == 3);
}

TEST_CASE("async_queue push move-only types", "[async_queue][push]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<std::unique_ptr<int>> queue(io_context);

  constexpr int test_value = 42;
  queue.push(std::make_unique<int>(test_value));

  CHECK_FALSE(queue.empty());
  REQUIRE(queue.size() == 1);
}

TEST_CASE("async_queue pop with coroutine from queue with values", "[async_queue][pop]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  constexpr int first_value = 10;
  constexpr int second_value = 20;
  queue.push(first_value);
  queue.push(second_value);

  auto result = std::make_shared<int>(0);
  auto completed = std::make_shared<bool>(false);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<int> result_ptr,
      std::shared_ptr<bool> completed_ptr) -> boost::asio::awaitable<void> {
      *result_ptr = co_await queue_ref.get().pop();
      *completed_ptr = true;
    }(std::ref(queue), result, completed),
    boost::asio::detached);

  io_context->run();

  CHECK(*completed);
  CHECK(*result == first_value);
  REQUIRE(queue.size() == 1);
  CHECK_FALSE(queue.empty());
}

TEST_CASE("async_queue pop suspends on empty queue and resumes when value pushed", "[async_queue][pop]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  auto completed = std::make_shared<bool>(false);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<bool> completed_ptr) -> boost::asio::awaitable<void> {
      co_await queue_ref.get().pop();
      *completed_ptr = true;
    }(std::ref(queue), completed),
    boost::asio::detached);

  io_context->poll();

  CHECK_FALSE(*completed);

  constexpr int pushed_value = 30;
  queue.push(pushed_value);
  io_context->run();

  CHECK(*completed);
}

TEST_CASE("async_queue concurrent multi-producer push with threads", "[async_queue][concurrent]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  constexpr int num_producers = 4;
  constexpr int items_per_producer = 100;
  constexpr int total_items = num_producers * items_per_producer;

  auto received_count = std::make_shared<int>(0);
  auto completed = std::make_shared<bool>(false);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<int> count_ptr,
      std::shared_ptr<bool> completed_ptr,
      int total) -> boost::asio::awaitable<void> {
      for (int idx = 0; idx < total; ++idx) {
        co_await queue_ref.get().pop();
        ++(*count_ptr);
      }
      *completed_ptr = true;
    }(std::ref(queue), received_count, completed, total_items),
    boost::asio::detached);

  std::vector<std::thread> producers;
  producers.reserve(num_producers);
  for (int producer_id = 0; producer_id < num_producers; ++producer_id) {
    producers.emplace_back([&queue, producer_id]() -> void {
      for (int idx = 0; idx < items_per_producer; ++idx) { queue.push((producer_id * items_per_producer) + idx); }
    });
  }

  for (auto &thread : producers) { thread.join(); }

  io_context->run();

  CHECK(*completed);
  CHECK(*received_count == total_items);
  CHECK(queue.empty());
}

TEST_CASE("async_queue concurrent multi-strand push", "[async_queue][concurrent]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  constexpr int num_strands = 3;
  constexpr int items_per_strand = 50;
  constexpr int total_items = num_strands * items_per_strand;

  auto received_count = std::make_shared<int>(0);
  auto completed = std::make_shared<bool>(false);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<int> count_ptr,
      std::shared_ptr<bool> completed_ptr,
      int total) -> boost::asio::awaitable<void> {
      for (int idx = 0; idx < total; ++idx) {
        co_await queue_ref.get().pop();
        ++(*count_ptr);
      }
      *completed_ptr = true;
    }(std::ref(queue), received_count, completed, total_items),
    boost::asio::detached);

  std::vector<boost::asio::strand<boost::asio::io_context::executor_type>> strands;
  strands.reserve(static_cast<std::size_t>(num_strands));
  for (int idx = 0; idx < num_strands; ++idx) { strands.emplace_back(boost::asio::make_strand(*io_context)); }

  for (int strand_id = 0; strand_id < num_strands; ++strand_id) {
    for (int idx = 0; idx < items_per_strand; ++idx) {
      boost::asio::post(strands[static_cast<std::size_t>(strand_id)],
        [&queue, strand_id, idx]() -> void { queue.push((strand_id * items_per_strand) + idx); });
    }
  }

  io_context->run();

  CHECK(*completed);
  CHECK(*received_count == total_items);
  CHECK(queue.empty());
}

struct cancellation_test_state
{
  std::shared_ptr<bool> was_cancelled;
  std::shared_ptr<bool> completed_normally;
};

TEST_CASE("async_queue pop respects cancellation signal", "[async_queue][cancellation]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
  auto state = std::make_shared<cancellation_test_state>(cancellation_test_state{
    .was_cancelled = std::make_shared<bool>(false), .completed_normally = std::make_shared<bool>(false) });

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot,
      std::shared_ptr<cancellation_test_state> tstate) -> boost::asio::awaitable<void> {
      try {
        co_await queue_ref.get().pop(c_slot);
        *tstate->completed_normally = true;
      } catch (const boost::system::system_error &e) {
        if (e.code() == boost::asio::error::operation_aborted
            or e.code() == boost::asio::experimental::error::channel_cancelled) {
          *tstate->was_cancelled = true;
        } else {
          throw;
        }
      }
    }(std::ref(queue), cancel_slot, state),
    boost::asio::detached);

  io_context->poll();

  CHECK_FALSE(*state->was_cancelled);
  CHECK_FALSE(*state->completed_normally);

  cancel_signal->emit(boost::asio::cancellation_type::terminal);
  io_context->run();

  CHECK(*state->was_cancelled);
  CHECK_FALSE(*state->completed_normally);
}

TEST_CASE("async_queue pop with cancellation slot and value available", "[async_queue][cancellation]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);
  constexpr int test_value = 42;
  queue.push(test_value);

  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
  auto result = std::make_shared<int>(0);
  auto completed = std::make_shared<bool>(false);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot,
      std::shared_ptr<int> result_ptr,
      std::shared_ptr<bool> completed_ptr) -> boost::asio::awaitable<void> {
      *result_ptr = co_await queue_ref.get().pop(c_slot);
      *completed_ptr = true;
    }(std::ref(queue), cancel_slot, result, completed),
    boost::asio::detached);

  io_context->run();

  CHECK(*completed);
  CHECK(*result == test_value);
}

TEST_CASE("async_queue pop without cancellation slot waits for value", "[async_queue][cancellation]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::async::async_queue<int> queue(io_context);

  auto completed = std::make_shared<bool>(false);
  auto result = std::make_shared<int>(0);

  boost::asio::co_spawn(
    *io_context,
    [](std::reference_wrapper<radix_relay::async::async_queue<int>> queue_ref,
      std::shared_ptr<int> result_ptr,
      std::shared_ptr<bool> completed_ptr) -> boost::asio::awaitable<void> {
      *result_ptr = co_await queue_ref.get().pop();
      *completed_ptr = true;
    }(std::ref(queue), result, completed),
    boost::asio::detached);

  io_context->poll();

  CHECK_FALSE(*completed);

  constexpr int pushed_value = 99;
  queue.push(pushed_value);
  io_context->run();

  CHECK(*completed);
  CHECK(*result == pushed_value);
}
