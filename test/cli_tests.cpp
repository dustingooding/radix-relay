#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/cli.hpp>
#include <core/events.hpp>
#include <string>
#include <utility>

TEST_CASE("interactive_cli can be constructed", "[cli][construction]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  std::ignore = radix_relay::core::interactive_cli{ "test-node", "hybrid", command_queue };
  REQUIRE(true);
}

TEST_CASE("interactive_cli command routing works correctly", "[cli][routing]")
{
  SECTION("should_quit identifies quit commands correctly")
  {
    REQUIRE(radix_relay::core::interactive_cli::should_quit("quit") == true);
    REQUIRE(radix_relay::core::interactive_cli::should_quit("exit") == true);
    REQUIRE(radix_relay::core::interactive_cli::should_quit("q") == true);
    REQUIRE(radix_relay::core::interactive_cli::should_quit("help") == false);
    REQUIRE(radix_relay::core::interactive_cli::should_quit("") == false);
    REQUIRE(radix_relay::core::interactive_cli::should_quit("version") == false);
  }

  SECTION("handle_command pushes commands to queue and handles mode switching")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto command_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
    radix_relay::core::interactive_cli cli("test-node", "hybrid", command_queue);

    REQUIRE(cli.handle_command("help") == true);
    REQUIRE(command_queue->size() == 1);

    REQUIRE(cli.handle_command("send alice hello") == true);
    REQUIRE(command_queue->size() == 2);

    REQUIRE(cli.handle_command("unknown_command") == true);
    REQUIRE(command_queue->size() == 3);
  }
}

TEST_CASE("interactive_cli mode handling works correctly", "[cli][mode]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  radix_relay::core::interactive_cli cli("test-node", "hybrid", command_queue);

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

TEST_CASE("interactive_cli command handlers execute safely", "[cli][handlers]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  radix_relay::core::interactive_cli cli("test-node", "hybrid", command_queue);

  SECTION("all commands are pushed to queue safely")
  {
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

    REQUIRE(command_queue->size() == 10);
  }

  SECTION("malformed commands are handled gracefully")
  {
    REQUIRE_NOTHROW(cli.handle_command("send alice hello world"));
    REQUIRE_NOTHROW(cli.handle_command("send alice"));
    REQUIRE_NOTHROW(cli.handle_command("send"));
    REQUIRE_NOTHROW(cli.handle_command("unknown_command"));
    REQUIRE_NOTHROW(cli.handle_command(""));

    REQUIRE(command_queue->size() == 5);
  }
}
