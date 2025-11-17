#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <core/presentation_processor.hpp>

struct test_presentation_event_handler
{
  std::vector<radix_relay::core::events::presentation_event_variant_t> handled_events;

  auto handle(const radix_relay::core::events::message_received &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::session_established &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::bundle_announcement_received &evt) -> void
  {
    handled_events.emplace_back(evt);
  }

  auto handle(const radix_relay::core::events::bundle_announcement_removed &evt) -> void
  {
    handled_events.emplace_back(evt);
  }

  auto handle(const radix_relay::core::events::message_sent &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::bundle_published &evt) -> void { handled_events.emplace_back(evt); }

  auto handle(const radix_relay::core::events::subscription_established &evt) -> void
  {
    handled_events.emplace_back(evt);
  }

  auto handle(const radix_relay::core::events::identities_listed &evt) -> void { handled_events.emplace_back(evt); }
};

SCENARIO("Transport event processor handles transport events", "[presentation_processor]")
{
  GIVEN("A transport event processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::presentation_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_presentation_event_handler>();
    auto processor = radix_relay::core::presentation_processor(io_context, event_queue, handler);

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

SCENARIO("Transport event processor handles multiple event types", "[presentation_processor]")
{
  GIVEN("A transport event processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::presentation_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_presentation_event_handler>();
    auto processor = radix_relay::core::presentation_processor(io_context, event_queue, handler);

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

SCENARIO("Transport event processor handles identities_listed events", "[presentation_processor]")
{
  GIVEN("A transport event processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::presentation_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_presentation_event_handler>();
    auto processor = radix_relay::core::presentation_processor(io_context, event_queue, handler);

    WHEN("an identities_listed event is pushed to the queue")
    {
      std::vector<radix_relay::core::events::discovered_identity> identities;
      identities.push_back(radix_relay::core::events::discovered_identity{
        .rdx_fingerprint = "RDX:abc123", .nostr_pubkey = "npub_alice", .event_id = "evt_alice" });
      identities.push_back(radix_relay::core::events::discovered_identity{
        .rdx_fingerprint = "RDX:def456", .nostr_pubkey = "npub_bob", .event_id = "evt_bob" });

      radix_relay::core::events::identities_listed evt{ .identities = identities };
      event_queue->push(evt);

      boost::asio::co_spawn(*io_context, processor.run_once(), boost::asio::detached);
      io_context->run();

      THEN("the handler processes the event")
      {
        REQUIRE(handler->handled_events.size() == 1);
        REQUIRE(std::holds_alternative<radix_relay::core::events::identities_listed>(handler->handled_events[0]));
        const auto &listed = std::get<radix_relay::core::events::identities_listed>(handler->handled_events[0]);
        REQUIRE(listed.identities.size() == 2);
        REQUIRE(listed.identities[0].rdx_fingerprint == "RDX:abc123");
        REQUIRE(listed.identities[0].nostr_pubkey == "npub_alice");
        REQUIRE(listed.identities[1].rdx_fingerprint == "RDX:def456");
        REQUIRE(listed.identities[1].nostr_pubkey == "npub_bob");
      }
    }
  }
}

SCENARIO("Transport event processor respects cancellation signal", "[presentation_processor][cancellation]")
{
  struct test_state
  {
    std::atomic<bool> coroutine_done{ false };
  };

  GIVEN("A transport event processor with cancellation support")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::presentation_event_variant_t>>(
        io_context);
    auto handler = std::make_shared<test_presentation_event_handler>();
    auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
    auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());
    auto processor = std::make_shared<radix_relay::core::presentation_processor<test_presentation_event_handler>>(
      io_context, event_queue, handler);

    auto state = std::make_shared<test_state>();

    WHEN("cancellation signal is emitted while processor is waiting")
    {
      boost::asio::co_spawn(
        *io_context,
        [](std::shared_ptr<radix_relay::core::presentation_processor<test_presentation_event_handler>> proc,
          std::shared_ptr<test_state> test_state_ptr,
          std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
          try {
            co_await proc->run(c_slot);
          } catch (const boost::system::system_error &err) {
            if (err.code() != boost::asio::error::operation_aborted
                and err.code() != boost::asio::experimental::error::channel_cancelled
                and err.code() != boost::asio::experimental::error::channel_closed) {
              throw;
            }
          }
          test_state_ptr->coroutine_done = true;
        }(processor, state, cancel_slot),
        boost::asio::detached);

      io_context->poll();

      cancel_signal->emit(boost::asio::cancellation_type::terminal);
      io_context->run();

      THEN("the processor should be cancelled")
      {
        REQUIRE(state->coroutine_done);
        CHECK(handler->handled_events.empty());
      }
    }
  }
}
