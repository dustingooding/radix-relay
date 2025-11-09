#include "test_doubles/test_double_websocket_stream.hpp"
#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <nostr/transport.hpp>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>
#include <vector>

namespace radix_relay::nostr::test {

SCENARIO("Queue-based transport processes connect command", "[nostr][transport][queue]")
{
  GIVEN("A transport constructed with queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(*io_context);

    auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

    WHEN("run_once processes a connect command from queue")
    {
      in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });

      boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

      io_context->run();

      THEN("WebSocket stream receives connection request")
      {
        const auto &connections = fake->get_connections();
        REQUIRE(connections.size() == 1);
        REQUIRE(connections[0].host == "relay.damus.io");
        REQUIRE(connections[0].port == "443");
        REQUIRE(connections[0].path == "/");
      }

      AND_THEN("Transport pushes connected event to out_queue with correct URL")
      {
        REQUIRE(out_queue->size() == 1);

        io_context->restart();
        auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
        io_context->run();
        auto event = future.get();

        REQUIRE(std::holds_alternative<core::events::transport::connected>(event));
        const auto &connected_evt = std::get<core::events::transport::connected>(event);
        REQUIRE(connected_evt.url == "wss://relay.damus.io");
      }
    }
  }
}

SCENARIO("Queue-based transport emits bytes_received events", "[nostr][transport][queue]")
{
  GIVEN("A connected transport with queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(*io_context);

    auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

    in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
    boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
    io_context->run();
    io_context->restart();

    REQUIRE(out_queue->size() == 1);
    auto clear_future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
    io_context->run();
    clear_future.get();

    WHEN("WebSocket stream receives data")
    {
      constexpr std::byte test_byte_1{ 0xAB };
      constexpr std::byte test_byte_2{ 0xCD };
      constexpr std::byte test_byte_3{ 0xEF };
      std::vector<std::byte> incoming_data{ test_byte_1, test_byte_2, test_byte_3 };
      fake->set_read_data(incoming_data);

      io_context->restart();
      io_context->run();

      THEN("Transport pushes bytes_received event to out_queue with correct data")
      {
        REQUIRE(out_queue->size() == 1);

        io_context->restart();
        auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
        io_context->run();
        auto event = future.get();

        REQUIRE(std::holds_alternative<core::events::transport::bytes_received>(event));
        const auto &received_evt = std::get<core::events::transport::bytes_received>(event);
        REQUIRE(received_evt.bytes == incoming_data);
      }
    }
  }
}

SCENARIO("Queue-based transport processes send command", "[nostr][transport][queue]")
{
  GIVEN("A connected transport with queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(*io_context);

    auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

    in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
    boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
    io_context->run();
    io_context->restart();

    REQUIRE(out_queue->size() == 1);
    auto clear_future2 = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
    io_context->run();
    clear_future2.get();

    WHEN("run_once processes a send command from queue")
    {
      std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
      in_queue->push(core::events::transport::send{ .message_id = "test-msg-id", .bytes = data });

      boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

      io_context->restart();
      io_context->run();

      THEN("WebSocket stream receives write request")
      {
        const auto &writes = fake->get_writes();
        REQUIRE(writes.size() == 1);
        REQUIRE(writes[0].data == data);
      }

      AND_THEN("Transport pushes sent event to out_queue with correct message ID")
      {
        REQUIRE(out_queue->size() == 1);

        io_context->restart();
        auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
        io_context->run();
        auto event = future.get();

        REQUIRE(std::holds_alternative<core::events::transport::sent>(event));
        const auto &sent_evt = std::get<core::events::transport::sent>(event);
        REQUIRE(sent_evt.message_id == "test-msg-id");
      }
    }
  }
}

SCENARIO("Queue-based transport processes disconnect command", "[nostr][transport][queue]")
{
  GIVEN("A connected transport with queues")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(*io_context);

    auto in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto out_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, in_queue, out_queue);

    in_queue->push(core::events::transport::connect{ .url = "wss://relay.damus.io" });
    boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);
    io_context->run();
    io_context->restart();

    REQUIRE(out_queue->size() == 1);
    auto clear_future3 = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
    io_context->run();
    clear_future3.get();

    WHEN("run_once processes a disconnect command from queue")
    {
      in_queue->push(core::events::transport::disconnect{});

      boost::asio::co_spawn(*io_context, transport.run_once(), boost::asio::detached);

      io_context->restart();
      io_context->run();

      THEN("Transport pushes disconnected event to out_queue")
      {
        REQUIRE(out_queue->size() == 1);

        io_context->restart();
        auto future = boost::asio::co_spawn(*io_context, out_queue->pop(), boost::asio::use_future);
        io_context->run();
        auto event = future.get();

        REQUIRE(std::holds_alternative<core::events::transport::disconnected>(event));
      }
    }
  }
}

}// namespace radix_relay::nostr::test
