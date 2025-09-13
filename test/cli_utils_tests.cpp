#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <vector>

#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>

TEST_CASE("CliArgs default values", "[cli_utils][cli_parser]")
{
  radix_relay::CliArgs args;

  REQUIRE(args.identity_path == "~/.radix/identity.key");
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
    radix_relay::CliArgs args;

    args.mode = "internet";
    REQUIRE(radix_relay::validate_cli_args(args) == true);

    args.mode = "mesh";
    REQUIRE(radix_relay::validate_cli_args(args) == true);

    args.mode = "hybrid";
    REQUIRE(radix_relay::validate_cli_args(args) == true);
  }

  SECTION("invalid modes fail validation")
  {
    radix_relay::CliArgs args;

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
  radix_relay::CliArgs args;

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
  radix_relay::CliArgs args;
  args.show_version = true;

  REQUIRE(radix_relay::execute_cli_command(args) == true);
}

TEST_CASE("execute_cli_command handles send command", "[cli_utils][app_init]")
{
  radix_relay::CliArgs args;
  args.send_parsed = true;
  args.send_recipient = "alice";
  args.send_message = "test message";

  REQUIRE(radix_relay::execute_cli_command(args) == true);
}

TEST_CASE("execute_cli_command handles peers command", "[cli_utils][app_init]")
{
  radix_relay::CliArgs args;
  args.peers_parsed = true;

  REQUIRE(radix_relay::execute_cli_command(args) == true);
}

TEST_CASE("execute_cli_command handles status command", "[cli_utils][app_init]")
{
  radix_relay::CliArgs args;
  args.status_parsed = true;

  REQUIRE(radix_relay::execute_cli_command(args) == true);
}

TEST_CASE("execute_cli_command returns false for no commands", "[cli_utils][app_init]")
{
  const radix_relay::CliArgs args;

  REQUIRE(radix_relay::execute_cli_command(args) == false);
}

TEST_CASE("configure_logging sets debug level when verbose", "[cli_utils][app_init]")
{
  SECTION("verbose flag enables debug logging")
  {
    radix_relay::CliArgs args;
    args.verbose = true;
    REQUIRE_NOTHROW(radix_relay::configure_logging(args));
  }

  SECTION("non-verbose flag does nothing")
  {
    radix_relay::CliArgs args;
    args.verbose = false;
    REQUIRE_NOTHROW(radix_relay::configure_logging(args));
  }
}

TEST_CASE("get_node_fingerprint returns valid fingerprint", "[cli_utils][app_init]")
{
  auto fingerprint = radix_relay::get_node_fingerprint();

  REQUIRE_FALSE(fingerprint.empty());
  REQUIRE(fingerprint.starts_with("RDX:"));
  REQUIRE(fingerprint.length() == 68);
}

TEST_CASE("AppState construction", "[cli_utils][app_init]")
{
  radix_relay::AppState state{
    .node_fingerprint = "RDX:test123", .mode = "hybrid", .identity_path = "~/.radix/test.key"
  };

  REQUIRE(state.node_fingerprint == "RDX:test123");
  REQUIRE(state.mode == "hybrid");
  REQUIRE(state.identity_path == "~/.radix/test.key");
}

TEST_CASE("print functions execute without throwing", "[cli_utils][app_init]")
{
  const radix_relay::AppState state{
    .node_fingerprint = "RDX:test123", .mode = "hybrid", .identity_path = "~/.radix/test.key"
  };

  SECTION("print_app_banner executes safely") { REQUIRE_NOTHROW(radix_relay::print_app_banner(state)); }

  SECTION("print_available_commands executes safely") { REQUIRE_NOTHROW(radix_relay::print_available_commands()); }
}
