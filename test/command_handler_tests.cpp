#include <catch2/catch_test_macros.hpp>

#include <radix_relay/command_handler.hpp>
#include <radix_relay/events/events.hpp>

SCENARIO("Command handler processes simple commands correctly", "[commands][handler][simple]")
{
  GIVEN("Simple commands")
  {
    WHEN("handling help command")
    {
      auto help_command = radix_relay::events::help{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(help_command));
      }
    }

    WHEN("handling version command")
    {
      auto version_command = radix_relay::events::version{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(version_command));
      }
    }

    WHEN("handling peers command")
    {
      auto peers_command = radix_relay::events::peers{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(peers_command));
      }
    }

    WHEN("handling status command")
    {
      auto status_command = radix_relay::events::status{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(status_command));
      }
    }

    WHEN("handling sessions command")
    {
      auto sessions_command = radix_relay::events::sessions{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(sessions_command));
      }
    }

    WHEN("handling scan command")
    {
      auto scan_command = radix_relay::events::scan{};

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(scan_command));
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
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(mode_command));
      }
    }

    WHEN("handling send command")
    {
      auto send_command = radix_relay::events::send{ .peer = "alice", .message = "hello world" };

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(send_command));
      }
    }

    WHEN("handling broadcast command")
    {
      auto broadcast_command = radix_relay::events::broadcast{ .message = "hello everyone" };

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(broadcast_command));
      }
    }

    WHEN("handling connect command")
    {
      auto connect_command = radix_relay::events::connect{ .relay = "wss://relay.damus.io" };

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(connect_command));
      }
    }

    WHEN("handling trust command")
    {
      auto trust_command = radix_relay::events::trust{ .peer = "alice" };

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(trust_command));
      }
    }

    WHEN("handling verify command")
    {
      auto verify_command = radix_relay::events::verify{ .peer = "bob" };

      THEN("handler should process without throwing")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(verify_command));
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
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(send_command));
      }
    }

    WHEN("handling broadcast command with empty message")
    {
      auto broadcast_command = radix_relay::events::broadcast{ .message = "" };

      THEN("handler should process gracefully")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(broadcast_command));
      }
    }

    WHEN("handling mode command with invalid mode")
    {
      auto mode_command = radix_relay::events::mode{ .new_mode = "invalid" };

      THEN("handler should process gracefully")
      {
        const radix_relay::CommandHandler handler;
        REQUIRE_NOTHROW(handler.handle(mode_command));
      }
    }
  }
}
