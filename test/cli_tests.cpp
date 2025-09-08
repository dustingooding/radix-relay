#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

#include "test_doubles/test_double_event_handler.hpp"
#include <radix_relay/cli.hpp>

TEST_CASE("InteractiveCli can be constructed", "[cli][construction]")
{
  const radix_relay_test::TestDoubleEventHandler test_event_handler;
  std::ignore = radix_relay::InteractiveCli{ "test-node", "hybrid", test_event_handler };
  REQUIRE(true);
}

TEST_CASE("InteractiveCli command routing works correctly", "[cli][routing]")
{
  SECTION("should_quit identifies quit commands correctly")
  {
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("quit") == true);
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("exit") == true);
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("q") == true);
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("help") == false);
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("") == false);
    REQUIRE(radix_relay::InteractiveCli<radix_relay_test::TestDoubleEventHandler>::should_quit("version") == false);
  }

  SECTION("handle_command delegates to EventHandler and handles mode switching")
  {
    const radix_relay_test::TestDoubleEventHandler test_event_handler;
    radix_relay::InteractiveCli cli("test-node", "hybrid", test_event_handler);

    test_event_handler.clear_handles();

    REQUIRE(cli.handle_command("help") == true);
    REQUIRE(test_event_handler.was_handled("help"));

    REQUIRE(cli.handle_command("send alice hello") == true);
    REQUIRE(test_event_handler.was_handled("send alice hello"));

    REQUIRE(cli.handle_command("unknown_command") == true);
    REQUIRE(test_event_handler.was_handled("unknown_command"));

    REQUIRE(test_event_handler.get_handle_count() == 3);
  }
}

TEST_CASE("InteractiveCli mode handling works correctly", "[cli][mode]")
{
  const radix_relay_test::TestDoubleEventHandler test_event_handler;
  radix_relay::InteractiveCli cli("test-node", "hybrid", test_event_handler);

  SECTION("mode can be switched to valid modes via handle_command")
  {
    REQUIRE(cli.handle_command("mode internet") == true);
    REQUIRE(cli.get_mode() == "internet");

    REQUIRE(cli.handle_command("mode mesh") == true);
    REQUIRE(cli.get_mode() == "mesh");

    REQUIRE(cli.handle_command("mode hybrid") == true);
    REQUIRE(cli.get_mode() == "hybrid");
  }

  SECTION("invalid mode is rejected and mode unchanged")
  {
    const std::string original_mode = cli.get_mode();

    REQUIRE(cli.handle_command("mode invalid") == true);
    REQUIRE(cli.get_mode() == original_mode);

    REQUIRE(cli.handle_command("mode ") == true);
    REQUIRE(cli.get_mode() == original_mode);

    REQUIRE(cli.handle_command("mode random") == true);
    REQUIRE(cli.get_mode() == original_mode);
  }
}

TEST_CASE("InteractiveCli command handlers execute safely", "[cli][handlers]")
{
  const radix_relay_test::TestDoubleEventHandler test_event_handler;
  radix_relay::InteractiveCli cli("test-node", "hybrid", test_event_handler);

  SECTION("all commands are delegated to EventHandler safely")
  {
    test_event_handler.clear_handles();

    REQUIRE_NOTHROW(cli.handle_command("help"));
    REQUIRE_NOTHROW(cli.handle_command("peers"));
    REQUIRE_NOTHROW(cli.handle_command("status"));
    REQUIRE_NOTHROW(cli.handle_command("sessions"));
    REQUIRE_NOTHROW(cli.handle_command("scan"));
    REQUIRE_NOTHROW(cli.handle_command("version"));
    REQUIRE_NOTHROW(cli.handle_command("broadcast test message"));
    REQUIRE_NOTHROW(cli.handle_command("connect wss://relay.damus.io"));
    REQUIRE_NOTHROW(cli.handle_command("trust alice"));
    REQUIRE_NOTHROW(cli.handle_command("verify alice"));

    REQUIRE(test_event_handler.get_handle_count() == 10);
  }

  SECTION("malformed commands are handled gracefully")
  {
    test_event_handler.clear_handles();

    REQUIRE_NOTHROW(cli.handle_command("send alice hello world"));
    REQUIRE_NOTHROW(cli.handle_command("send alice"));
    REQUIRE_NOTHROW(cli.handle_command("send"));
    REQUIRE_NOTHROW(cli.handle_command("unknown_command"));
    REQUIRE_NOTHROW(cli.handle_command(""));

    REQUIRE(test_event_handler.get_handle_count() == 5);
  }
}
