#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <tuple>
#include <vector>

#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/platform/env_utils.hpp>

TEST_CASE("cli_args default values", "[cli_utils][cli_parser]")
{
  radix_relay::cli_args args;

  REQUIRE(args.identity_path == "~/.radix/identity.db");
  REQUIRE(args.mode == "hybrid");
  REQUIRE(args.verbose == false);
  REQUIRE(args.show_version == false);
  REQUIRE(args.send_parsed == false);
  REQUIRE(args.peers_parsed == false);
  REQUIRE(args.status_parsed == false);
  REQUIRE(args.send_recipient.empty());
  REQUIRE(args.send_message.empty());
}

TEST_CASE("validate_cli_args validates mode", "[cli_utils][cli_parser]")
{
  SECTION("valid modes pass validation")
  {
    radix_relay::cli_args args;

    args.mode = "internet";
    REQUIRE(radix_relay::validate_cli_args(args) == true);

    args.mode = "mesh";
    REQUIRE(radix_relay::validate_cli_args(args) == true);

    args.mode = "hybrid";
    REQUIRE(radix_relay::validate_cli_args(args) == true);
  }

  SECTION("invalid modes fail validation")
  {
    radix_relay::cli_args args;

    args.mode = "invalid";
    REQUIRE(radix_relay::validate_cli_args(args) == false);

    args.mode = "";
    REQUIRE(radix_relay::validate_cli_args(args) == false);

    args.mode = "HYBRID";
    REQUIRE(radix_relay::validate_cli_args(args) == false);
  }
}

TEST_CASE("validate_cli_args validates send command", "[cli_utils][cli_parser]")
{
  radix_relay::cli_args args;

  SECTION("send command with valid args passes")
  {
    args.send_parsed = true;
    args.send_recipient = "alice";
    args.send_message = "hello";

    REQUIRE(radix_relay::validate_cli_args(args) == true);
  }

  SECTION("send command with empty recipient fails")
  {
    args.send_parsed = true;
    args.send_recipient = "";
    args.send_message = "hello";

    REQUIRE(radix_relay::validate_cli_args(args) == false);
  }

  SECTION("send command with empty message fails")
  {
    args.send_parsed = true;
    args.send_recipient = "alice";
    args.send_message = "";

    REQUIRE(radix_relay::validate_cli_args(args) == false);
  }

  SECTION("non-send commands ignore send validation")
  {
    args.send_parsed = false;
    args.send_recipient = "";
    args.send_message = "";

    REQUIRE(radix_relay::validate_cli_args(args) == true);
  }
}

TEST_CASE("execute_cli_command handles version flag", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_version.db").string());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  radix_relay::cli_args args;
  args.show_version = true;

  REQUIRE(radix_relay::execute_cli_command(args, command_handler) == true);
  std::ignore =
    std::remove((std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_version.db")
        .string()
        .c_str());
}

TEST_CASE("execute_cli_command handles send command", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_send.db").string());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  radix_relay::cli_args args;
  args.send_parsed = true;
  args.send_recipient = "alice";
  args.send_message = "test message";

  REQUIRE(radix_relay::execute_cli_command(args, command_handler) == true);
  std::ignore = std::remove(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_send.db").string().c_str());
}

TEST_CASE("execute_cli_command handles peers command", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_peers.db").string());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  radix_relay::cli_args args;
  args.peers_parsed = true;

  REQUIRE(radix_relay::execute_cli_command(args, command_handler) == true);
  std::ignore =
    std::remove((std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_peers.db")
        .string()
        .c_str());
}

TEST_CASE("execute_cli_command handles status command", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_status.db").string());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  radix_relay::cli_args args;
  args.status_parsed = true;

  REQUIRE(radix_relay::execute_cli_command(args, command_handler) == true);
  std::ignore =
    std::remove((std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_status.db")
        .string()
        .c_str());
}

TEST_CASE("execute_cli_command returns false for no commands", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_false.db").string());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  const radix_relay::cli_args args;

  REQUIRE(radix_relay::execute_cli_command(args, command_handler) == false);
  std::ignore =
    std::remove((std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_execute_cli_false.db")
        .string()
        .c_str());
}

TEST_CASE("configure_logging sets debug level when verbose", "[cli_utils][app_init]")
{
  SECTION("verbose flag enables debug logging")
  {
    radix_relay::cli_args args;
    args.verbose = true;
    REQUIRE_NOTHROW(radix_relay::configure_logging(args));
  }

  SECTION("non-verbose flag does nothing")
  {
    radix_relay::cli_args args;
    args.verbose = false;
    REQUIRE_NOTHROW(radix_relay::configure_logging(args));
  }
}

TEST_CASE("get_node_fingerprint returns valid fingerprint", "[cli_utils][app_init]")
{
  auto bridge = radix_relay::new_signal_bridge(
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_cli_utils_fingerprint.db").string());
  auto fingerprint = radix_relay::get_node_fingerprint(*bridge);

  REQUIRE_FALSE(fingerprint.empty());
  REQUIRE(fingerprint.starts_with("RDX:"));
  REQUIRE(fingerprint.length() == 68);

  // Clean up test database
  std::ignore =
    std::remove((std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_cli_utils_fingerprint.db")
        .string()
        .c_str());
}

TEST_CASE("app_state construction", "[cli_utils][app_init]")
{
  radix_relay::app_state state{
    .node_fingerprint = "RDX:test123", .mode = "hybrid", .identity_path = "~/.radix/test.key"
  };

  REQUIRE(state.node_fingerprint == "RDX:test123");
  REQUIRE(state.mode == "hybrid");
  REQUIRE(state.identity_path == "~/.radix/test.key");
}

TEST_CASE("print functions execute without throwing", "[cli_utils][app_init]")
{
  const radix_relay::app_state state{
    .node_fingerprint = "RDX:test123", .mode = "hybrid", .identity_path = "~/.radix/test.key"
  };

  SECTION("print_app_banner executes safely") { REQUIRE_NOTHROW(radix_relay::print_app_banner(state)); }

  SECTION("print_available_commands executes safely") { REQUIRE_NOTHROW(radix_relay::print_available_commands()); }
}
