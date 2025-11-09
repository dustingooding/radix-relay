#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <async/async_queue.hpp>

SCENARIO("async_queue can be constructed", "[async_queue][construction]")
{
  GIVEN("An io_context")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();

    WHEN("constructing an async_queue with int type")
    {
      const radix_relay::async::async_queue<int> queue(io_context);

      THEN("the queue should be empty") { REQUIRE(queue.empty()); }
    }

    WHEN("constructing an async_queue with string type")
    {
      const radix_relay::async::async_queue<std::string> queue(io_context);

      THEN("the queue should be empty") { REQUIRE(queue.empty()); }
    }
  }
}

SCENARIO("async_queue push operation", "[async_queue][push]")
{
  GIVEN("An empty queue")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<int> queue(io_context);

    WHEN("pushing a value")
    {
      constexpr int test_value = 42;
      queue.push(test_value);

      THEN("the queue should not be empty") { REQUIRE_FALSE(queue.empty()); }

      THEN("the queue size should be 1") { REQUIRE(queue.size() == 1); }
    }

    WHEN("pushing multiple values")
    {
      queue.push(1);
      queue.push(2);
      queue.push(3);

      THEN("the queue should not be empty") { REQUIRE_FALSE(queue.empty()); }

      THEN("the queue size should be 3") { REQUIRE(queue.size() == 3); }
    }
  }

  GIVEN("A queue with move-only types")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<std::unique_ptr<int>> queue(io_context);

    WHEN("pushing a unique_ptr")
    {
      constexpr int test_value = 42;
      queue.push(std::make_unique<int>(test_value));

      THEN("the queue should not be empty") { REQUIRE_FALSE(queue.empty()); }

      THEN("the queue size should be 1") { REQUIRE(queue.size() == 1); }
    }
  }
}

SCENARIO("async_queue pop operation with coroutine", "[async_queue][pop]")
{
  GIVEN("A queue with values")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<int> queue(io_context);

    constexpr int first_value = 10;
    constexpr int second_value = 20;
    queue.push(first_value);
    queue.push(second_value);

    WHEN("popping values with co_await")
    {
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

      THEN("the coroutine should complete")
      {
        REQUIRE(*completed);
        REQUIRE(*result == first_value);
      }

      THEN("the queue size should decrease")
      {
        REQUIRE(queue.size() == 1);
        REQUIRE_FALSE(queue.empty());
      }
    }
  }

  GIVEN("An empty queue")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<int> queue(io_context);

    WHEN("attempting to pop from empty queue")
    {
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

      THEN("the coroutine should suspend") { REQUIRE_FALSE(*completed); }

      AND_WHEN("a value is pushed")
      {
        constexpr int pushed_value = 30;
        queue.push(pushed_value);
        io_context->run();

        THEN("the coroutine should resume and complete") { REQUIRE(*completed); }
      }
    }
  }
}

SCENARIO("async_queue handles concurrent multi-producer push", "[async_queue][concurrent]")
{
  GIVEN("A queue with multiple producer threads")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<int> queue(io_context);

    constexpr int num_producers = 4;
    constexpr int items_per_producer = 100;
    constexpr int total_items = num_producers * items_per_producer;

    WHEN("multiple threads push concurrently")
    {
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
        producers.emplace_back([&queue, producer_id]() {
          for (int idx = 0; idx < items_per_producer; ++idx) { queue.push((producer_id * items_per_producer) + idx); }
        });
      }

      for (auto &thread : producers) { thread.join(); }

      io_context->run();

      THEN("all items should be received")
      {
        REQUIRE(*completed);
        REQUIRE(*received_count == total_items);
      }

      THEN("the queue should be empty") { REQUIRE(queue.empty()); }
    }
  }

  GIVEN("A queue with multiple strands pushing")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    radix_relay::async::async_queue<int> queue(io_context);

    constexpr int num_strands = 3;
    constexpr int items_per_strand = 50;
    constexpr int total_items = num_strands * items_per_strand;

    WHEN("multiple strands post push operations")
    {
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
            [&queue, strand_id, idx]() { queue.push((strand_id * items_per_strand) + idx); });
        }
      }

      io_context->run();

      THEN("all items should be received")
      {
        REQUIRE(*completed);
        REQUIRE(*received_count == total_items);
      }

      THEN("the queue should be empty") { REQUIRE(queue.empty()); }
    }
  }
}
