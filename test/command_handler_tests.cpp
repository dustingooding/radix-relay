#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tuple>

#include "test_doubles/test_double_printer.hpp"
#include "test_doubles/test_double_signal_bridge.hpp"
#include <radix_relay/core/command_handler.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <radix_relay/signal/node_identity.hpp>
#include <radix_relay/signal/signal_bridge.hpp>

SCENARIO("Command handler processes simple commands correctly", "[commands][handler][simple]")
{
  GIVEN("Simple commands")
  {
    WHEN("handling help command")
    {
      auto help_command = radix_relay::core::events::help{};

      THEN("handler outputs available commands")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(help_command);
        REQUIRE(test_printer->get_output().find("Interactive Commands") != std::string::npos);
      }
    }

    WHEN("handling version command")
    {
      auto version_command = radix_relay::core::events::version{};

      THEN("handler outputs version information")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(version_command);
        REQUIRE(test_printer->get_output().find("Radix Relay v") != std::string::npos);
      }
    }

    WHEN("handling peers command")
    {
      auto peers_command = radix_relay::core::events::peers{};

      THEN("handler outputs peer discovery information")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(peers_command);
        REQUIRE(test_printer->get_output().find("Connected Peers") != std::string::npos);
      }
    }

    WHEN("handling status command")
    {
      auto status_command = radix_relay::core::events::status{};

      THEN("handler outputs network and crypto status")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(status_command);
        const auto output = test_printer->get_output();
        REQUIRE(output.find("Network Status") != std::string::npos);
        REQUIRE(output.find("Node Fingerprint") != std::string::npos);
        REQUIRE(output.find("RDX:") != std::string::npos);
      }
    }

    WHEN("handling sessions command with no sessions")
    {
      auto sessions_command = radix_relay::core::events::sessions{};

      THEN("handler outputs no active sessions message")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        bridge->contacts_to_return = {};
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(sessions_command);
        REQUIRE(test_printer->get_output().find("No active sessions") != std::string::npos);
      }
    }

    WHEN("handling sessions command with established sessions")
    {
      auto sessions_command = radix_relay::core::events::sessions{};

      THEN("handler outputs active sessions with contact information")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        bridge->contacts_to_return = {
          radix_relay::signal::contact_info{
            .rdx_fingerprint = "RDX:alice123",
            .nostr_pubkey = "npub_alice",
            .user_alias = "Alice",
            .has_active_session = true,
          },
          radix_relay::signal::contact_info{
            .rdx_fingerprint = "RDX:bob456",
            .nostr_pubkey = "npub_bob",
            .user_alias = "",
            .has_active_session = true,
          },
        };
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(sessions_command);
        const auto output = test_printer->get_output();
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
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(scan_command);
        REQUIRE(test_printer->get_output().find("Scanning") != std::string::npos);
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
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(mode_command);
        REQUIRE(test_printer->get_output().find("internet") != std::string::npos);
      }
    }

    WHEN("handling send command")
    {
      auto send_command = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };

      THEN("handler outputs send command confirmation with peer and message")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(send_command);
        const auto output = test_printer->get_output();
        REQUIRE(output.find("alice") != std::string::npos);
        REQUIRE(output.find("hello world") != std::string::npos);
      }
    }

    WHEN("handling broadcast command")
    {
      auto broadcast_command = radix_relay::core::events::broadcast{ .message = "hello everyone" };

      THEN("handler outputs broadcast command confirmation with message")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(broadcast_command);
        REQUIRE(test_printer->get_output().find("hello everyone") != std::string::npos);
      }
    }

    WHEN("handling connect command")
    {
      auto connect_command = radix_relay::core::events::connect{ .relay = "wss://relay.damus.io" };

      THEN("handler outputs connect command confirmation with relay URL")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(connect_command);
        REQUIRE(test_printer->get_output().find("relay.damus.io") != std::string::npos);
      }
    }

    WHEN("handling trust command")
    {
      auto trust_command = radix_relay::core::events::trust{ .peer = "alice", .alias = "" };

      THEN("handler outputs trust command confirmation with peer")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(trust_command);
        REQUIRE(test_printer->get_output().find("alice") != std::string::npos);
      }
    }

    WHEN("handling verify command")
    {
      auto verify_command = radix_relay::core::events::verify{ .peer = "bob" };

      THEN("handler outputs verify command confirmation with peer")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(verify_command);
        REQUIRE(test_printer->get_output().find("bob") != std::string::npos);
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
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(send_command);
        REQUIRE(test_printer->get_output().find("Usage") != std::string::npos);
      }
    }

    WHEN("handling broadcast command with empty message")
    {
      auto broadcast_command = radix_relay::core::events::broadcast{ .message = "" };

      THEN("handler outputs usage information")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(broadcast_command);
        REQUIRE(test_printer->get_output().find("Usage") != std::string::npos);
      }
    }

    WHEN("handling mode command with invalid mode")
    {
      auto mode_command = radix_relay::core::events::mode{ .new_mode = "invalid" };

      THEN("handler outputs invalid mode error message")
      {
        auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
        auto test_printer = std::make_shared<radix_relay_test::test_double_printer>();
        const radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge,
          radix_relay_test::test_double_printer>
          handler{ bridge, test_printer };
        handler.handle(mode_command);
        REQUIRE(test_printer->get_output().find("Invalid mode") != std::string::npos);
      }
    }
  }
}
