#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tuple>

#include "test_doubles/test_double_signal_bridge.hpp"
#include <radix_relay/command_handler.hpp>
#include <radix_relay/events/events.hpp>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <radix_relay/signal_bridge.hpp>

SCENARIO("Command handler processes simple commands correctly", "[commands][handler][simple]")
{
  GIVEN("Simple commands")
  {
    WHEN("handling help command")
    {
      auto help_command = radix_relay::events::help{};

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_help.db";
        {
          auto bridge = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge };
          REQUIRE_NOTHROW(handler.handle(help_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling version command")
    {
      auto version_command = radix_relay::events::version{};

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_version.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(version_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling peers command")
    {
      auto peers_command = radix_relay::events::peers{};

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_peers.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(peers_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling status command")
    {
      auto status_command = radix_relay::events::status{};

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_status.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(status_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling sessions command with no sessions")
    {
      auto sessions_command = radix_relay::events::sessions{};

      THEN("handler should process without throwing")
      {
        auto db_path = std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_sessions_empty.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(sessions_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling sessions command with established sessions")
    {
      auto sessions_command = radix_relay::events::sessions{};

      THEN("handler should process without throwing")
      {
        auto alice_db = std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_sessions_alice.db";
        auto bob_db = std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_sessions_bob.db";
        {
          auto alice_wrapper = std::make_shared<radix_relay::signal::bridge>(alice_db);
          auto bob_wrapper = std::make_shared<radix_relay::signal::bridge>(bob_db);

          auto bob_bundle_json = bob_wrapper->generate_prekey_bundle_announcement("test-0.1.0");
          auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
          const std::string bob_bundle_base64 = bob_event_json["content"].template get<std::string>();

          alice_wrapper->add_contact_and_establish_session_from_base64(bob_bundle_base64, "");

          const radix_relay::command_handler handler{ alice_wrapper };
          REQUIRE_NOTHROW(handler.handle(sessions_command));
        }
        std::ignore = std::filesystem::remove(alice_db);
        std::ignore = std::filesystem::remove(bob_db);
      }
    }

    WHEN("handling scan command")
    {
      auto scan_command = radix_relay::events::scan{};

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_scan.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(scan_command));
        }
        std::ignore = std::filesystem::remove(db_path);
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
      auto mode_command = radix_relay::events::mode{ .new_mode = "internet" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_mode.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(mode_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling send command")
    {
      auto send_command = radix_relay::events::send{ .peer = "alice", .message = "hello world" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_send.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(send_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling broadcast command")
    {
      auto broadcast_command = radix_relay::events::broadcast{ .message = "hello everyone" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_broadcast.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(broadcast_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling connect command")
    {
      auto connect_command = radix_relay::events::connect{ .relay = "wss://relay.damus.io" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_connect.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(connect_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling trust command")
    {
      auto trust_command = radix_relay::events::trust{ .peer = "alice", .alias = "" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_trust.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(trust_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling verify command")
    {
      auto verify_command = radix_relay::events::verify{ .peer = "bob" };

      THEN("handler should process without throwing")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_verify.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(verify_command));
        }
        std::ignore = std::filesystem::remove(db_path);
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
      auto send_command = radix_relay::events::send{ .peer = "", .message = "" };

      THEN("handler should process gracefully")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_send_empty.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(send_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling broadcast command with empty message")
    {
      auto broadcast_command = radix_relay::events::broadcast{ .message = "" };

      THEN("handler should process gracefully")
      {
        auto db_path = std::filesystem::path(radix_relay::platform::get_temp_directory())
                       / "test_command_handler_broadcast_empty.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(broadcast_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }

    WHEN("handling mode command with invalid mode")
    {
      auto mode_command = radix_relay::events::mode{ .new_mode = "invalid" };

      THEN("handler should process gracefully")
      {
        auto db_path =
          std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_command_handler_mode_invalid.db";
        {
          auto bridge_wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
          const radix_relay::command_handler handler{ bridge_wrapper };
          REQUIRE_NOTHROW(handler.handle(mode_command));
        }
        std::ignore = std::filesystem::remove(db_path);
      }
    }
  }
}

SCENARIO("Command handler calls bridge methods correctly", "[commands][handler][bridge]")
{
  GIVEN("A test double signal bridge")
  {
    auto bridge = std::make_shared<radix_relay_test::TestDoubleSignalBridge>();

    WHEN("sessions command is executed with no sessions")
    {
      const radix_relay::command_handler handler{ bridge };
      handler.handle(radix_relay::events::sessions{});

      THEN("list_contacts should be called")
      {
        REQUIRE(bridge->was_called("list_contacts"));
        REQUIRE(bridge->call_count("list_contacts") == 1);
      }
    }

    WHEN("sessions command is executed with sessions")
    {
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

      const radix_relay::command_handler handler{ bridge };
      handler.handle(radix_relay::events::sessions{});

      THEN("list_contacts should be called once")
      {
        REQUIRE(bridge->was_called("list_contacts"));
        REQUIRE(bridge->call_count("list_contacts") == 1);
      }
    }

    WHEN("status command is executed")
    {
      const radix_relay::command_handler handler{ bridge };
      handler.handle(radix_relay::events::status{});

      THEN("get_node_fingerprint should be called")
      {
        REQUIRE(bridge->was_called("get_node_fingerprint"));
        REQUIRE(bridge->call_count("get_node_fingerprint") == 1);
      }
    }
  }
}
