#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/core/transport_event_processor.hpp>

struct test_transport_event_handler
{
  std::vector<radix_relay::core::events::transport_event_variant_t> handled_events;

  auto handle(const radix_relay::core::events::message_received &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::session_established &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::bundle_announcement_received &evt) -> void
  {
    handled_events.emplace_back(evt);
  }

  auto handle(const radix_relay::core::events::message_sent &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::bundle_published &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::subscription_established &evt) -> void
  {
    handled_events.emplace_back(evt);
  }
};

SCENARIO("Transport event processor handles transport events", "[transport_event_processor]")
{
  GIVEN("A transport event processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::transport_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_transport_event_handler>();
    auto processor = radix_relay::core::transport_event_processor(io_context, event_queue, handler);

    WHEN("a message_received event is pushed to the queue")
    {
      constexpr std::uint64_t test_timestamp = 12345;
      radix_relay::core::events::message_received evt{
        .sender_rdx = "RDX:alice123", .content = "hello", .timestamp = test_timestamp
      };
      event_queue->push(evt);

      boost::asio::co_spawn(*io_context, processor.run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler processes the event")
      {
        REQUIRE(handler->handled_events.size() == 1);
        REQUIRE(std::holds_alternative<radix_relay::core::events::message_received>(handler->handled_events[0]));
        const auto &received = std::get<radix_relay::core::events::message_received>(handler->handled_events[0]);
        REQUIRE(received.sender_rdx == "RDX:alice123");
        REQUIRE(received.content == "hello");
        REQUIRE(received.timestamp == 12345);
      }
    }
  }
}

SCENARIO("Transport event processor handles multiple event types", "[transport_event_processor]")
{
  GIVEN("A transport event processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::transport_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_transport_event_handler>();
    auto processor = radix_relay::core::transport_event_processor(io_context, event_queue, handler);

    WHEN("multiple different events are pushed")
    {
      event_queue->push(radix_relay::core::events::session_established{ .peer_rdx = "RDX:bob456" });
      event_queue->push(
        radix_relay::core::events::message_sent{ .peer = "alice", .event_id = "evt123", .accepted = true });
      event_queue->push(radix_relay::core::events::subscription_established{ .subscription_id = "sub123" });

      boost::asio::co_spawn(*io_context, processor.run_once(), boost::asio::detached);
      boost::asio::co_spawn(*io_context, processor.run_once(), boost::asio::detached);
      boost::asio::co_spawn(*io_context, processor.run_once(), boost::asio::detached);
      io_context->run();

      THEN("all events are processed")
      {
        REQUIRE(handler->handled_events.size() == 3);
        REQUIRE(std::holds_alternative<radix_relay::core::events::session_established>(handler->handled_events[0]));
        REQUIRE(std::holds_alternative<radix_relay::core::events::message_sent>(handler->handled_events[1]));
        REQUIRE(
          std::holds_alternative<radix_relay::core::events::subscription_established>(handler->handled_events[2]));
      }
    }
  }
}
