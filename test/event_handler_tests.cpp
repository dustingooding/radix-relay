#include <catch2/catch_test_macros.hpp>

#include "test_doubles/test_double_command_handler.hpp"
#include <radix_relay/event_handler.hpp>
#include <radix_relay/events/events.hpp>

SCENARIO("Event handler processes raw command events correctly", "[events][handler][raw_command]")
{
  GIVEN("A test command handler")
  {
    auto test_cmd_handler = std::make_shared<radix_relay_test::TestDoubleCommandHandler>();
    const radix_relay::event_handler event_handler{ test_cmd_handler };

    WHEN("handling simple raw commands")
    {
      auto help_event = radix_relay::events::raw_command{ .input = "help" };
      auto version_event = radix_relay::events::raw_command{ .input = "version" };
      auto peers_event = radix_relay::events::raw_command{ .input = "peers" };

      THEN("handler should parse and route commands correctly")
      {
        test_cmd_handler->clear_calls();

        event_handler.handle(help_event);
        REQUIRE(test_cmd_handler->was_called("help"));

        event_handler.handle(version_event);
        REQUIRE(test_cmd_handler->was_called("version"));

        event_handler.handle(peers_event);
        REQUIRE(test_cmd_handler->was_called("peers"));

        REQUIRE(test_cmd_handler->get_call_count() == 3);
      }
    }

    WHEN("handling parameterized raw commands")
    {
      auto mode_event = radix_relay::events::raw_command{ .input = "mode internet" };
      auto send_event = radix_relay::events::raw_command{ .input = "send alice hello" };
      auto broadcast_event = radix_relay::events::raw_command{ .input = "broadcast test message" };

      THEN("handler should parse parameters correctly and route to command handler")
      {
        test_cmd_handler->clear_calls();

        event_handler.handle(mode_event);
        REQUIRE(test_cmd_handler->was_called("mode:internet"));

        event_handler.handle(send_event);
        REQUIRE(test_cmd_handler->was_called("send:alice:hello"));

        event_handler.handle(broadcast_event);
        REQUIRE(test_cmd_handler->was_called("broadcast:test message"));

        REQUIRE(test_cmd_handler->get_call_count() == 3);
      }
    }

    WHEN("handling unknown raw commands")
    {
      auto unknown_event = radix_relay::events::raw_command{ .input = "unknown_command" };

      THEN("handler should process gracefully without routing to command handler")
      {
        test_cmd_handler->clear_calls();

        REQUIRE_NOTHROW(event_handler.handle(unknown_event));
        REQUIRE(test_cmd_handler->get_call_count() == 0);
      }
    }

    WHEN("handling malformed raw commands")
    {
      auto empty_event = radix_relay::events::raw_command{ .input = "" };
      auto incomplete_send = radix_relay::events::raw_command{ .input = "send alice" };
      auto incomplete_mode = radix_relay::events::raw_command{ .input = "mode" };

      THEN("handler should process gracefully and route malformed send commands")
      {
        test_cmd_handler->clear_calls();

        REQUIRE_NOTHROW(event_handler.handle(empty_event));
        REQUIRE_NOTHROW(event_handler.handle(incomplete_send));
        REQUIRE_NOTHROW(event_handler.handle(incomplete_mode));

        REQUIRE(test_cmd_handler->was_called("send::"));
      }
    }
  }
}
