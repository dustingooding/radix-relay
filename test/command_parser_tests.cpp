#include <catch2/catch_test_macros.hpp>
#include <core/command_parser.hpp>
#include <core/events.hpp>

#include "test_doubles/test_double_signal_bridge.hpp"

using radix_relay::core::command_parser;
using radix_relay::core::events::broadcast;
using radix_relay::core::events::chat;
using radix_relay::core::events::connect;
using radix_relay::core::events::disconnect;
using radix_relay::core::events::help;
using radix_relay::core::events::identities;
using radix_relay::core::events::leave;
using radix_relay::core::events::mode;
using radix_relay::core::events::peers;
using radix_relay::core::events::publish_identity;
using radix_relay::core::events::scan;
using radix_relay::core::events::send;
using radix_relay::core::events::sessions;
using radix_relay::core::events::status;
using radix_relay::core::events::trust;
using radix_relay::core::events::unknown_command;
using radix_relay::core::events::unpublish_identity;
using radix_relay::core::events::verify;
using radix_relay::core::events::version;

using bridge_t = radix_relay_test::test_double_signal_bridge;
using parser_t = command_parser<bridge_t>;

static auto make_parser() -> parser_t { return parser_t(std::make_shared<bridge_t>()); }

TEST_CASE("command_parser parses simple commands", "[command_parser]")
{
  const auto parser = make_parser();

  SECTION("help command")
  {
    auto result = parser.parse("/help");
    CHECK(std::holds_alternative<help>(result));
  }

  SECTION("peers command")
  {
    auto result = parser.parse("/peers");
    CHECK(std::holds_alternative<peers>(result));
  }

  SECTION("status command")
  {
    auto result = parser.parse("/status");
    CHECK(std::holds_alternative<status>(result));
  }

  SECTION("sessions command")
  {
    auto result = parser.parse("/sessions");
    CHECK(std::holds_alternative<sessions>(result));
  }

  SECTION("identities command")
  {
    auto result = parser.parse("/identities");
    CHECK(std::holds_alternative<identities>(result));
  }

  SECTION("scan command")
  {
    auto result = parser.parse("/scan");
    CHECK(std::holds_alternative<scan>(result));
  }

  SECTION("version command")
  {
    auto result = parser.parse("/version");
    CHECK(std::holds_alternative<version>(result));
  }

  SECTION("publish command")
  {
    auto result = parser.parse("/publish");
    CHECK(std::holds_alternative<publish_identity>(result));
  }

  SECTION("unpublish command")
  {
    auto result = parser.parse("/unpublish");
    CHECK(std::holds_alternative<unpublish_identity>(result));
  }

  SECTION("disconnect command")
  {
    auto result = parser.parse("/disconnect");
    CHECK(std::holds_alternative<disconnect>(result));
  }

  SECTION("leave command")
  {
    auto result = parser.parse("/leave");
    CHECK(std::holds_alternative<leave>(result));
  }
}

TEST_CASE("command_parser parses commands with arguments", "[command_parser]")
{
  const auto parser = make_parser();

  SECTION("mode command")
  {
    auto result = parser.parse("/mode internet");
    REQUIRE(std::holds_alternative<mode>(result));
    CHECK(std::get<mode>(result).new_mode == "internet");
  }

  SECTION("send command with peer and message")
  {
    auto result = parser.parse("/send alice hello world");
    REQUIRE(std::holds_alternative<send>(result));
    const auto &cmd = std::get<send>(result);
    CHECK(cmd.peer == "alice");
    CHECK(cmd.message == "hello world");
  }

  SECTION("send command with peer only returns empty fields")
  {
    auto result = parser.parse("/send alice");
    REQUIRE(std::holds_alternative<send>(result));
    const auto &cmd = std::get<send>(result);
    CHECK(cmd.peer.empty());
    CHECK(cmd.message.empty());
  }

  SECTION("broadcast command")
  {
    auto result = parser.parse("/broadcast hello everyone");
    REQUIRE(std::holds_alternative<broadcast>(result));
    CHECK(std::get<broadcast>(result).message == "hello everyone");
  }

  SECTION("connect command")
  {
    auto result = parser.parse("/connect wss://relay.example.com");
    REQUIRE(std::holds_alternative<connect>(result));
    CHECK(std::get<connect>(result).relay == "wss://relay.example.com");
  }

  SECTION("trust command with peer and alias")
  {
    auto result = parser.parse("/trust RDX:abc123 Alice");
    REQUIRE(std::holds_alternative<trust>(result));
    const auto &cmd = std::get<trust>(result);
    CHECK(cmd.peer == "RDX:abc123");
    CHECK(cmd.alias == "Alice");
  }

  SECTION("trust command with peer only")
  {
    auto result = parser.parse("/trust RDX:abc123");
    REQUIRE(std::holds_alternative<trust>(result));
    const auto &cmd = std::get<trust>(result);
    CHECK(cmd.peer == "RDX:abc123");
    CHECK(cmd.alias.empty());
  }

  SECTION("verify command")
  {
    auto result = parser.parse("/verify RDX:abc123");
    REQUIRE(std::holds_alternative<verify>(result));
    CHECK(std::get<verify>(result).peer == "RDX:abc123");
  }

  SECTION("chat command")
  {
    auto result = parser.parse("/chat alice");
    REQUIRE(std::holds_alternative<chat>(result));
    CHECK(std::get<chat>(result).contact == "alice");
  }
}

TEST_CASE("command_parser returns unknown_command for unrecognized input", "[command_parser]")
{
  const auto parser = make_parser();

  SECTION("unrecognized slash command")
  {
    auto result = parser.parse("/unknown");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "/unknown");
  }

  SECTION("empty input")
  {
    auto result = parser.parse("");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input.empty());
  }

  SECTION("plain text without slash")
  {
    auto result = parser.parse("hello world");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "hello world");
  }

  SECTION("whitespace only")
  {
    auto result = parser.parse("   ");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "   ");
  }

  SECTION("partial command match")
  {
    auto result = parser.parse("/hel");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "/hel");
  }

  SECTION("command without required space for args")
  {
    auto result = parser.parse("/modeinternet");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "/modeinternet");
  }
}

TEST_CASE("command_parser chat mode behavior", "[command_parser][chat_mode]")
{
  auto bridge = std::make_shared<bridge_t>();
  const parser_t parser(bridge);

  SECTION("parser starts not in chat mode") { CHECK_FALSE(parser.in_chat_mode()); }

  SECTION("enter_chat_mode sets chat mode")
  {
    parser.enter_chat_mode("RDX:alice123");
    CHECK(parser.in_chat_mode());
  }

  SECTION("exit_chat_mode clears chat mode")
  {
    parser.enter_chat_mode("RDX:alice123");
    parser.exit_chat_mode();
    CHECK_FALSE(parser.in_chat_mode());
  }

  SECTION("plain text becomes send command in chat mode")
  {
    parser.enter_chat_mode("RDX:alice123");
    auto result = parser.parse("hello world");
    REQUIRE(std::holds_alternative<send>(result));
    const auto &cmd = std::get<send>(result);
    CHECK(cmd.peer == "RDX:alice123");
    CHECK(cmd.message == "hello world");
  }

  SECTION("slash commands are not affected by chat mode")
  {
    parser.enter_chat_mode("RDX:alice123");
    auto result = parser.parse("/help");
    CHECK(std::holds_alternative<help>(result));
  }

  SECTION("/leave command exits chat mode")
  {
    parser.enter_chat_mode("RDX:alice123");
    auto result = parser.parse("/leave");
    CHECK(std::holds_alternative<leave>(result));
    CHECK_FALSE(parser.in_chat_mode());
  }

  SECTION("plain text after /leave is unknown_command")
  {
    parser.enter_chat_mode("RDX:alice123");
    std::ignore = parser.parse("/leave");
    auto result = parser.parse("hello");
    REQUIRE(std::holds_alternative<unknown_command>(result));
    CHECK(std::get<unknown_command>(result).input == "hello");
  }
}

TEST_CASE("command_parser /chat command enters chat mode via bridge lookup", "[command_parser][chat_mode]")
{
  auto bridge = std::make_shared<bridge_t>();
  bridge->contacts_to_return.push_back(radix_relay::core::contact_info{ .rdx_fingerprint = "RDX:alice123",
    .nostr_pubkey = "npub_alice",
    .user_alias = "alice",
    .has_active_session = true });

  const parser_t parser(bridge);

  SECTION("/chat with valid contact enters chat mode")
  {
    CHECK_FALSE(parser.in_chat_mode());
    auto result = parser.parse("/chat alice");
    REQUIRE(std::holds_alternative<chat>(result));
    CHECK(std::get<chat>(result).contact == "alice");
    CHECK(parser.in_chat_mode());
  }

  SECTION("/chat uses rdx_fingerprint from bridge lookup for send commands")
  {
    std::ignore = parser.parse("/chat alice");
    auto result = parser.parse("hello world");
    REQUIRE(std::holds_alternative<send>(result));
    const auto &cmd = std::get<send>(result);
    CHECK(cmd.peer == "RDX:alice123");
    CHECK(cmd.message == "hello world");
  }

  SECTION("/chat with unknown contact does not enter chat mode")
  {
    auto result = parser.parse("/chat unknown_user");
    REQUIRE(std::holds_alternative<chat>(result));
    CHECK(std::get<chat>(result).contact == "unknown_user");
    CHECK_FALSE(parser.in_chat_mode());
  }
}
