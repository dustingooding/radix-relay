#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include <radix_relay/cli_utils/cli_parser.hpp>

auto create_argv(std::vector<std::string> &args) -> std::vector<char *>
{
  std::vector<char *> argv;
  argv.reserve(args.size());
  for (auto &arg : args) { argv.push_back(arg.data()); }
  return argv;
}

TEST_CASE("CLI parsing basic flags", "[cli_utils][cli_parser][integration]")
{
  SECTION("version flag sets show_version")
  {
    std::vector<std::string> args = { "radix-relay", "--version" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.show_version == true);
  }

  SECTION("verbose flag sets verbose")
  {
    std::vector<std::string> args = { "radix-relay", "--verbose" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.verbose == true);
  }

  SECTION("short verbose flag works")
  {
    std::vector<std::string> args = { "radix-relay", "-v" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.verbose == true);
  }
}

TEST_CASE("CLI parsing options", "[cli_utils][cli_parser][integration]")
{
  SECTION("identity path option")
  {
    std::vector<std::string> args = { "radix-relay", "--identity", "/custom/path.key" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.identity_path == "/custom/path.key");
  }

  SECTION("short identity option")
  {
    std::vector<std::string> args = { "radix-relay", "-i", "/short/path.key" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.identity_path == "/short/path.key");
  }

  SECTION("mode option - internet")
  {
    std::vector<std::string> args = { "radix-relay", "--mode", "internet" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.mode == "internet");
  }

  SECTION("mode option - mesh")
  {
    std::vector<std::string> args = { "radix-relay", "-m", "mesh" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.mode == "mesh");
  }
}

TEST_CASE("CLI parsing send subcommand", "[cli_utils][cli_parser][integration]")
{
  SECTION("send command with recipient and message")
  {
    std::vector<std::string> args = { "radix-relay", "send", "alice", "hello world" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.send_parsed == true);
    REQUIRE(parsed.send_recipient == "alice");
    REQUIRE(parsed.send_message == "hello world");
  }

  SECTION("send command with flags")
  {
    std::vector<std::string> args = { "radix-relay", "-v", "--identity", "/test.key", "send", "bob", "test message" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.verbose == true);
    REQUIRE(parsed.identity_path == "/test.key");
    REQUIRE(parsed.send_parsed == true);
    REQUIRE(parsed.send_recipient == "bob");
    REQUIRE(parsed.send_message == "test message");
  }
}

TEST_CASE("CLI parsing other subcommands", "[cli_utils][cli_parser][integration]")
{
  SECTION("peers subcommand")
  {
    std::vector<std::string> args = { "radix-relay", "peers" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.peers_parsed == true);
  }

  SECTION("status subcommand")
  {
    std::vector<std::string> args = { "radix-relay", "status" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.status_parsed == true);
  }

  SECTION("status with verbose")
  {
    std::vector<std::string> args = { "radix-relay", "-v", "status" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.verbose == true);
    REQUIRE(parsed.status_parsed == true);
  }
}

TEST_CASE("CLI parsing defaults", "[cli_utils][cli_parser][integration]")
{
  SECTION("no arguments uses defaults")
  {
    std::vector<std::string> args = { "radix-relay" };
    auto argv = create_argv(args);

    auto parsed = radix_relay::cli_utils::parse_cli_args(static_cast<int>(args.size()), argv.data());

    REQUIRE(parsed.identity_path.ends_with("/.radix/identity.db"));
    REQUIRE(parsed.mode == "hybrid");
    REQUIRE(parsed.verbose == false);
    REQUIRE(parsed.show_version == false);
    REQUIRE(parsed.send_parsed == false);
    REQUIRE(parsed.peers_parsed == false);
    REQUIRE(parsed.status_parsed == false);
  }
}
