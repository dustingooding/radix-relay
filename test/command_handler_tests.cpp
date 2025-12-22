#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tuple>

#include "test_doubles/test_double_signal_bridge.hpp"
#include <async/async_queue.hpp>
#include <core/command_handler.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>
#include <platform/env_utils.hpp>
#include <signal/signal_bridge.hpp>

struct command_handler_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::display_message>> display_out_queue;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::transport::in_t>> transport_out_queue;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::session_orchestrator::in_t>>
    session_out_queue;
  std::shared_ptr<radix_relay_test::test_double_signal_bridge> bridge;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::connection_monitor::in_t>>
    connection_monitor_queue;
  radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge> handler;

  command_handler_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      display_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context)),
      transport_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::transport::in_t>>(io_context)),
      session_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::session_orchestrator::in_t>>(
          io_context)),
      bridge(std::make_shared<radix_relay_test::test_double_signal_bridge>()),
      connection_monitor_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::connection_monitor::in_t>>(
          io_context)),
      handler(bridge,
        radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge>::out_queues_t{
          .display = display_out_queue,
          .transport = transport_out_queue,
          .session = session_out_queue,
          .connection_monitor = connection_monitor_queue })
  {}

  [[nodiscard]] auto get_all_output() const -> std::string
  {
    std::string result;
    while (auto msg = display_out_queue->try_pop()) { result += msg->message; }
    return result;
  }
};

SCENARIO("Command handler processes simple commands correctly", "[commands][handler][simple]")
{
  GIVEN("Simple commands")
  {
    WHEN("handling help command")
    {
      auto help_command = radix_relay::core::events::help{};

      THEN("handler emits display_message event with available commands")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(help_command);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("Interactive Commands") != std::string::npos);
      }
    }

    WHEN("handling version command")
    {
      auto version_command = radix_relay::core::events::version{};

      THEN("handler outputs version information")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(version_command);
        REQUIRE(fixture.get_all_output().find("Radix Relay v") != std::string::npos);
      }
    }

    WHEN("handling peers command")
    {
      auto peers_command = radix_relay::core::events::peers{};

      THEN("handler outputs peer discovery information")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(peers_command);
        REQUIRE(fixture.get_all_output().find("Connected Peers") != std::string::npos);
      }
    }

    WHEN("handling status command")
    {
      auto status_command = radix_relay::core::events::status{};

      THEN("handler forwards query to connection monitor and outputs crypto status")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(status_command);

        auto monitor_event = fixture.connection_monitor_queue->try_pop();
        REQUIRE(monitor_event.has_value());
        if (monitor_event.has_value()) {
          CHECK(std::holds_alternative<radix_relay::core::events::connection_monitor::query_status>(*monitor_event));
        }

        const auto output = fixture.get_all_output();
        CHECK(output.find("Node Fingerprint") != std::string::npos);
        CHECK(output.find("RDX:") != std::string::npos);
      }
    }

    WHEN("handling sessions command with no sessions")
    {
      auto sessions_command = radix_relay::core::events::sessions{};

      THEN("handler outputs no active sessions message")
      {
        const command_handler_fixture fixture;
        fixture.bridge->contacts_to_return = {};
        fixture.handler.handle(sessions_command);
        REQUIRE(fixture.get_all_output().find("No active sessions") != std::string::npos);
      }
    }

    WHEN("handling sessions command with established sessions")
    {
      auto sessions_command = radix_relay::core::events::sessions{};

      THEN("handler outputs active sessions with contact information")
      {
        const command_handler_fixture fixture;
        fixture.bridge->contacts_to_return = {
          radix_relay::core::contact_info{
            .rdx_fingerprint = "RDX:alice123",
            .nostr_pubkey = "npub_alice",
            .user_alias = "Alice",
            .has_active_session = true,
          },
          radix_relay::core::contact_info{
            .rdx_fingerprint = "RDX:bob456",
            .nostr_pubkey = "npub_bob",
            .user_alias = "",
            .has_active_session = true,
          },
        };
        fixture.handler.handle(sessions_command);
        const auto output = fixture.get_all_output();
        REQUIRE(output.find("Active Sessions") != std::string::npos);
        REQUIRE(output.find("Alice") != std::string::npos);
        REQUIRE(output.find("RDX:alice123") != std::string::npos);
        REQUIRE(output.find("RDX:bob456") != std::string::npos);
      }
    }

    WHEN("handling scan command")
    {
      auto scan_command = radix_relay::core::events::scan{};

      THEN("handler outputs scan information")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(scan_command);
        REQUIRE(fixture.get_all_output().find("Scanning") != std::string::npos);
      }
    }

    WHEN("handling identities command")
    {
      auto identities_command = radix_relay::core::events::identities{};

      THEN("handler pushes list_identities event to session queue")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(identities_command);

        auto session_event = fixture.session_out_queue->try_pop();
        REQUIRE(session_event.has_value());
        if (session_event) {
          REQUIRE(std::holds_alternative<radix_relay::core::events::list_identities>(*session_event));
        }
      }
    }
  }
}

SCENARIO("Command handler processes parameterized commands correctly", "[commands][handler][parameterized]")
{
  GIVEN("Parameterized commands")
  {
    WHEN("handling mode command")
    {
      auto mode_command = radix_relay::core::events::mode{ .new_mode = "internet" };

      THEN("handler outputs mode change confirmation")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(mode_command);
        REQUIRE(fixture.get_all_output().find("internet") != std::string::npos);
      }
    }

    WHEN("handling send command")
    {
      auto send_command = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };

      THEN("handler pushes send event to session queue")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(send_command);

        auto session_event = fixture.session_out_queue->try_pop();
        REQUIRE(session_event.has_value());
        if (session_event) {
          REQUIRE(std::holds_alternative<radix_relay::core::events::send>(*session_event));
          const auto &send_event = std::get<radix_relay::core::events::send>(*session_event);
          REQUIRE(send_event.peer == "alice");
          REQUIRE(send_event.message == "hello world");
        }
      }

      THEN("handler outputs send command confirmation with peer and message")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(send_command);
        const auto output = fixture.get_all_output();
        REQUIRE(output.find("alice") != std::string::npos);
        REQUIRE(output.find("hello world") != std::string::npos);
      }
    }

    WHEN("handling broadcast command")
    {
      auto broadcast_command = radix_relay::core::events::broadcast{ .message = "hello everyone" };

      THEN("handler outputs broadcast command confirmation with message")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(broadcast_command);
        REQUIRE(fixture.get_all_output().find("hello everyone") != std::string::npos);
      }
    }

    WHEN("handling connect command")
    {
      auto connect_command = radix_relay::core::events::connect{ .relay = "wss://relay.damus.io" };

      THEN("handler outputs connect command confirmation with relay URL")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(connect_command);
        REQUIRE(fixture.get_all_output().find("relay.damus.io") != std::string::npos);
      }

      THEN("handler pushes connect event to session queue")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(connect_command);

        auto session_event = fixture.session_out_queue->try_pop();
        REQUIRE(session_event.has_value());
        if (session_event) {
          REQUIRE(std::holds_alternative<radix_relay::core::events::connect>(*session_event));
          const auto &connect_event = std::get<radix_relay::core::events::connect>(*session_event);
          REQUIRE(connect_event.relay == "wss://relay.damus.io");
        }
      }
    }

    WHEN("handling disconnect command")
    {
      auto disconnect_command = radix_relay::core::events::disconnect{};

      THEN("handler outputs disconnect confirmation")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(disconnect_command);
        REQUIRE(fixture.get_all_output().find("Disconnecting") != std::string::npos);
      }

      THEN("handler pushes transport disconnect event to transport queue")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(disconnect_command);

        auto transport_event = fixture.transport_out_queue->try_pop();
        REQUIRE(transport_event.has_value());
        if (transport_event) {
          REQUIRE(std::holds_alternative<radix_relay::core::events::transport::disconnect>(*transport_event));
        }
      }
    }

    WHEN("handling trust command")
    {
      auto trust_command = radix_relay::core::events::trust{ .peer = "RDX:alice123", .alias = "Alice" };

      THEN("handler pushes trust event to session queue")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(trust_command);

        auto session_event = fixture.session_out_queue->try_pop();
        REQUIRE(session_event.has_value());
        if (session_event) {
          REQUIRE(std::holds_alternative<radix_relay::core::events::trust>(*session_event));
          const auto &trust = std::get<radix_relay::core::events::trust>(*session_event);
          REQUIRE(trust.peer == "RDX:alice123");
          REQUIRE(trust.alias == "Alice");
        }

        REQUIRE(fixture.get_all_output().find("RDX:alice123") != std::string::npos);
      }
    }

    WHEN("handling verify command")
    {
      auto verify_command = radix_relay::core::events::verify{ .peer = "bob" };

      THEN("handler outputs verify command confirmation with peer")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(verify_command);
        REQUIRE(fixture.get_all_output().find("bob") != std::string::npos);
      }
    }
  }
}

SCENARIO("Command handler validates command parameters", "[commands][handler][validation]")
{
  GIVEN("Commands with invalid parameters")
  {
    WHEN("handling send command with empty parameters")
    {
      auto send_command = radix_relay::core::events::send{ .peer = "", .message = "" };

      THEN("handler outputs usage information")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(send_command);
        REQUIRE(fixture.get_all_output().find("Usage") != std::string::npos);
      }
    }

    WHEN("handling broadcast command with empty message")
    {
      auto broadcast_command = radix_relay::core::events::broadcast{ .message = "" };

      THEN("handler outputs usage information")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(broadcast_command);
        REQUIRE(fixture.get_all_output().find("Usage") != std::string::npos);
      }
    }

    WHEN("handling mode command with invalid mode")
    {
      auto mode_command = radix_relay::core::events::mode{ .new_mode = "invalid" };

      THEN("handler outputs invalid mode error message")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(mode_command);
        REQUIRE(fixture.get_all_output().find("Invalid mode") != std::string::npos);
      }
    }
  }
}
