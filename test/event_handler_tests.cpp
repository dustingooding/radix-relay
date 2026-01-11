#include <catch2/catch_test_macros.hpp>

#include "test_doubles/test_double_command_handler.hpp"
#include <core/event_handler.hpp>
#include <core/events.hpp>

SCENARIO("Event handler processes raw command events correctly", "[events][handler][raw_command]")
{
  GIVEN("A test command handler")
  {
    auto test_cmd_handler = std::make_shared<radix_relay_test::test_double_command_handler>();
    radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t const queues{};
    const radix_relay::core::event_handler event_handler{ test_cmd_handler, queues };

    WHEN("handling simple raw commands")
    {
      auto help_event = radix_relay::core::events::raw_command{ .input = "/help" };
      auto version_event = radix_relay::core::events::raw_command{ .input = "/version" };
      auto peers_event = radix_relay::core::events::raw_command{ .input = "/peers" };
      auto identities_event = radix_relay::core::events::raw_command{ .input = "/identities" };
      auto publish_event = radix_relay::core::events::raw_command{ .input = "/publish" };

      THEN("handler should parse and route commands correctly")
      {
        test_cmd_handler->clear_calls();

        event_handler.handle(help_event);
        REQUIRE(test_cmd_handler->was_called("help"));

        event_handler.handle(version_event);
        REQUIRE(test_cmd_handler->was_called("version"));

        event_handler.handle(peers_event);
        REQUIRE(test_cmd_handler->was_called("peers"));

        event_handler.handle(identities_event);
        REQUIRE(test_cmd_handler->was_called("identities"));

        event_handler.handle(publish_event);
        REQUIRE(test_cmd_handler->was_called("publish_identity"));

        REQUIRE(test_cmd_handler->get_call_count() == 5);
      }
    }

    WHEN("handling parameterized raw commands")
    {
      auto mode_event = radix_relay::core::events::raw_command{ .input = "/mode internet" };
      auto send_event = radix_relay::core::events::raw_command{ .input = "/send alice hello" };
      auto broadcast_event = radix_relay::core::events::raw_command{ .input = "/broadcast test message" };
      auto connect_event = radix_relay::core::events::raw_command{ .input = "/connect wss://relay.example.com" };
      auto disconnect_event = radix_relay::core::events::raw_command{ .input = "/disconnect" };

      THEN("handler should parse parameters correctly and route to command handler")
      {
        test_cmd_handler->clear_calls();

        event_handler.handle(mode_event);
        REQUIRE(test_cmd_handler->was_called("mode:internet"));

        event_handler.handle(send_event);
        REQUIRE(test_cmd_handler->was_called("send:alice:hello"));

        event_handler.handle(broadcast_event);
        REQUIRE(test_cmd_handler->was_called("broadcast:test message"));

        event_handler.handle(connect_event);
        REQUIRE(test_cmd_handler->was_called("connect:wss://relay.example.com"));

        event_handler.handle(disconnect_event);
        REQUIRE(test_cmd_handler->was_called("disconnect"));

        REQUIRE(test_cmd_handler->get_call_count() == 5);
      }
    }

    WHEN("handling unknown raw commands")
    {
      auto unknown_event = radix_relay::core::events::raw_command{ .input = "unknown_command" };

      THEN("handler should process gracefully without routing to command handler")
      {
        test_cmd_handler->clear_calls();

        REQUIRE_NOTHROW(event_handler.handle(unknown_event));
        REQUIRE(test_cmd_handler->get_call_count() == 0);
      }
    }

    WHEN("handling malformed raw commands")
    {
      auto empty_event = radix_relay::core::events::raw_command{ .input = "" };
      auto incomplete_send = radix_relay::core::events::raw_command{ .input = "/send alice" };
      auto incomplete_mode = radix_relay::core::events::raw_command{ .input = "/mode" };

      THEN("handler should process gracefully and route malformed send commands")
      {
        test_cmd_handler->clear_calls();

        REQUIRE_NOTHROW(event_handler.handle(empty_event));
        REQUIRE_NOTHROW(event_handler.handle(incomplete_send));
        REQUIRE_NOTHROW(event_handler.handle(incomplete_mode));

        REQUIRE(test_cmd_handler->was_called("send::"));
      }
    }

    WHEN("handling chat context commands")
    {
      auto chat_event = radix_relay::core::events::raw_command{ .input = "/chat alice" };
      auto leave_event = radix_relay::core::events::raw_command{ .input = "/leave" };

      THEN("handler should parse and route chat commands correctly")
      {
        test_cmd_handler->clear_calls();

        event_handler.handle(chat_event);
        REQUIRE(test_cmd_handler->was_called("chat:alice"));

        event_handler.handle(leave_event);
        REQUIRE(test_cmd_handler->was_called("leave"));

        REQUIRE(test_cmd_handler->get_call_count() == 2);
      }
    }
  }
}

SCENARIO("Event handler preprocesses messages in chat mode", "[events][handler][chat_mode]")
{
  GIVEN("An event handler with a contact available")
  {
    auto test_cmd_handler = std::make_shared<radix_relay_test::test_double_command_handler>();
    test_cmd_handler->bridge->contacts.push_back(radix_relay::core::contact_info{ .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "alice",
      .has_active_session = true });
    radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t const queues{};
    const radix_relay::core::event_handler event_handler{ test_cmd_handler, queues };

    WHEN("entering chat mode via /chat command")
    {
      event_handler.handle(radix_relay::core::events::raw_command{ .input = "/chat alice" });

      AND_WHEN("user types plain text without slash")
      {
        auto plain_text = radix_relay::core::events::raw_command{ .input = "hello" };
        event_handler.handle(plain_text);

        THEN("handler should preprocess it as a send command to active contact")
        {
          REQUIRE(test_cmd_handler->was_called("send:RDX:alice123:hello"));
        }
      }

      AND_WHEN("user types a slash command")
      {
        auto slash_command = radix_relay::core::events::raw_command{ .input = "/help" };
        event_handler.handle(slash_command);

        THEN("handler should not preprocess it")
        {
          REQUIRE(test_cmd_handler->was_called("help"));
          REQUIRE_FALSE(test_cmd_handler->was_called("send"));
        }
      }

      AND_WHEN("exiting chat mode via /leave command")
      {
        event_handler.handle(radix_relay::core::events::raw_command{ .input = "/leave" });

        AND_WHEN("user types plain text")
        {
          auto plain_text = radix_relay::core::events::raw_command{ .input = "hello" };
          event_handler.handle(plain_text);

          THEN("handler should not preprocess it") { REQUIRE_FALSE(test_cmd_handler->was_called("send")); }
        }
      }
    }
  }
}
