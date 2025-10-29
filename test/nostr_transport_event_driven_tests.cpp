#include "test_doubles/test_double_websocket_stream.hpp"
#include <radix_relay/core/events.hpp>
#include <radix_relay/nostr/transport.hpp>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <variant>
#include <vector>

namespace radix_relay::nostr::test {

SCENARIO("Event-driven transport handles connect command", "[nostr][transport][events]")
{
  GIVEN("An io_context with transport and session strands")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    WHEN("Transport receives connect command on transport strand")
    {
      std::vector<core::events::transport::event_variant_t> received_events;

      auto send_event = [&received_events](
                          core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

      transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, &session_strand, send_event);

      const core::events::transport::connect cmd{ .url = "wss://relay.damus.io" };
      boost::asio::post(transport_strand, [&transport, cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.run();

      THEN("WebSocket stream receives connection request")
      {
        const auto &connections = fake->get_connections();
        REQUIRE(connections.size() == 1);
        REQUIRE(connections[0].host == "relay.damus.io");
        REQUIRE(connections[0].port == "443");
        REQUIRE(connections[0].path == "/");
      }

      AND_THEN("Transport emits connected event")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::connected>(received_events[0]));
        const auto &connected_evt = std::get<core::events::transport::connected>(received_events[0]);
        REQUIRE(connected_evt.url == "wss://relay.damus.io");
      }
    }
  }
}

SCENARIO("Event-driven transport handles send command", "[nostr][transport][events]")
{
  GIVEN("A connected transport")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    std::vector<core::events::transport::event_variant_t> received_events;

    auto send_event = [&received_events](
                        core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, &session_strand, send_event);

    const core::events::transport::connect connect_cmd{ .url = "wss://relay.damus.io" };
    boost::asio::post(transport_strand, [&transport, connect_cmd]() {// NOLINT(bugprone-exception-escape)
      try {
        transport.handle_command(connect_cmd);
      } catch (const std::exception &e) {
        spdlog::error("[test] handle_command failed: {}", e.what());
      }
    });
    io_context.restart();
    io_context.run();

    WHEN("Transport receives send command on transport strand")
    {
      received_events.clear();

      std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
      const core::events::transport::send send_cmd{ .message_id = "test-msg-id", .bytes = data };
      boost::asio::post(transport_strand, [&transport, send_cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(send_cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.restart();
      io_context.run();

      THEN("WebSocket stream receives write request")
      {
        const auto &writes = fake->get_writes();
        REQUIRE(writes.size() == 1);
        REQUIRE(writes[0].data == data);
      }

      AND_THEN("Transport emits sent event with message ID")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::sent>(received_events[0]));
        const auto &sent_evt = std::get<core::events::transport::sent>(received_events[0]);
        REQUIRE(sent_evt.message_id == "test-msg-id");
      }
    }
  }
}

SCENARIO("Event-driven transport emits bytes_received event", "[nostr][transport][events]")
{
  GIVEN("A connected transport")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    std::vector<core::events::transport::event_variant_t> received_events;

    auto send_event = [&received_events](
                        core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, &session_strand, send_event);

    const core::events::transport::connect connect_cmd{ .url = "wss://relay.damus.io" };
    boost::asio::post(transport_strand, [&transport, connect_cmd]() {// NOLINT(bugprone-exception-escape)
      try {
        transport.handle_command(connect_cmd);
      } catch (const std::exception &e) {
        spdlog::error("[test] handle_command failed: {}", e.what());
      }
    });
    io_context.restart();
    io_context.run();

    WHEN("WebSocket stream receives data")
    {
      received_events.clear();

      constexpr std::byte test_byte_1{ 0xAB };
      constexpr std::byte test_byte_2{ 0xCD };
      constexpr std::byte test_byte_3{ 0xEF };
      std::vector<std::byte> incoming_data{ test_byte_1, test_byte_2, test_byte_3 };
      fake->set_read_data(incoming_data);

      io_context.restart();
      io_context.run();

      THEN("Transport emits bytes_received event")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::bytes_received>(received_events[0]));
        const auto &received_evt = std::get<core::events::transport::bytes_received>(received_events[0]);
        REQUIRE(received_evt.bytes == incoming_data);
      }
    }
  }
}

SCENARIO("Event-driven transport handles disconnect command", "[nostr][transport][events]")
{
  GIVEN("A connected transport")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    std::vector<core::events::transport::event_variant_t> received_events;

    auto send_event = [&received_events](
                        core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, &session_strand, send_event);

    const core::events::transport::connect connect_cmd{ .url = "wss://relay.damus.io" };
    boost::asio::post(transport_strand, [&transport, connect_cmd]() {// NOLINT(bugprone-exception-escape)
      try {
        transport.handle_command(connect_cmd);
      } catch (const std::exception &e) {
        spdlog::error("[test] handle_command failed: {}", e.what());
      }
    });
    io_context.restart();
    io_context.run();

    WHEN("Transport receives disconnect command on transport strand")
    {
      received_events.clear();

      const core::events::transport::disconnect disconnect_cmd{};
      boost::asio::post(transport_strand, [&transport, disconnect_cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(disconnect_cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.restart();
      io_context.run();

      THEN("Transport emits disconnected event")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::disconnected>(received_events[0]));
      }
    }
  }
}

SCENARIO("Event-driven transport emits connect_failed event", "[nostr][transport][events]")
{
  GIVEN("An io_context with transport and session strands")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    WHEN("Transport receives connect command and connection fails")
    {
      std::vector<core::events::transport::event_variant_t> received_events;

      auto send_event = [&received_events](
                          core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

      transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, &session_strand, send_event);

      fake->set_connect_failure(true);

      const core::events::transport::connect cmd{ .url = "wss://relay.damus.io" };
      boost::asio::post(transport_strand, [&transport, cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.run();

      THEN("Transport emits connect_failed event")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::connect_failed>(received_events[0]));
        const auto &failed_evt = std::get<core::events::transport::connect_failed>(received_events[0]);
        REQUIRE(failed_evt.url == "wss://relay.damus.io");
        REQUIRE(not failed_evt.error_message.empty());
      }
    }
  }
}

SCENARIO("Event-driven transport emits send_failed event on write failure", "[nostr][transport][events]")
{
  GIVEN("A connected transport")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    std::vector<core::events::transport::event_variant_t> received_events;

    auto send_event = [&received_events](
                        core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, &session_strand, send_event);

    const core::events::transport::connect connect_cmd{ .url = "wss://relay.damus.io" };
    boost::asio::post(transport_strand, [&transport, connect_cmd]() {// NOLINT(bugprone-exception-escape)
      try {
        transport.handle_command(connect_cmd);
      } catch (const std::exception &e) {
        spdlog::error("[test] handle_command failed: {}", e.what());
      }
    });
    io_context.restart();
    io_context.run();

    WHEN("Transport receives send command but write fails")
    {
      received_events.clear();

      fake->set_write_failure(true);

      const std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
      const core::events::transport::send send_cmd{ .message_id = "test-msg-id", .bytes = data };
      boost::asio::post(transport_strand, [&transport, send_cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(send_cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.restart();
      io_context.run();

      THEN("Transport emits send_failed event with message ID")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::send_failed>(received_events[0]));
        const auto &failed_evt = std::get<core::events::transport::send_failed>(received_events[0]);
        REQUIRE(failed_evt.message_id == "test-msg-id");
        REQUIRE(not failed_evt.error_message.empty());
      }
    }
  }
}

SCENARIO("Event-driven transport emits send_failed event when not connected", "[nostr][transport][events]")
{
  GIVEN("A disconnected transport")
  {
    boost::asio::io_context io_context;
    const boost::asio::io_context::strand transport_strand{ io_context };
    const boost::asio::io_context::strand session_strand{ io_context };
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    std::vector<core::events::transport::event_variant_t> received_events;

    auto send_event = [&received_events](
                        core::events::transport::event_variant_t evt) { received_events.push_back(std::move(evt)); };

    transport<radix_relay::test::test_double_websocket_stream> transport(fake, io_context, &session_strand, send_event);

    WHEN("Transport receives send command without being connected")
    {
      const std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 } };
      const core::events::transport::send send_cmd{ .message_id = "test-msg-id", .bytes = data };
      boost::asio::post(transport_strand, [&transport, send_cmd]() {// NOLINT(bugprone-exception-escape)
        try {
          transport.handle_command(send_cmd);
        } catch (const std::exception &e) {
          spdlog::error("[test] handle_command failed: {}", e.what());
        }
      });

      io_context.run();

      THEN("Transport emits send_failed event with 'Not connected' error")
      {
        REQUIRE(received_events.size() == 1);
        REQUIRE(std::holds_alternative<core::events::transport::send_failed>(received_events[0]));
        const auto &failed_evt = std::get<core::events::transport::send_failed>(received_events[0]);
        REQUIRE(failed_evt.message_id == "test-msg-id");
        REQUIRE(failed_evt.error_message == "Not connected");
      }
    }
  }
}

}// namespace radix_relay::nostr::test
