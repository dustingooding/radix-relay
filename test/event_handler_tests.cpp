#include <catch2/catch_test_macros.hpp>

#include "test_doubles/test_double_command_handler.hpp"
#include <core/event_handler.hpp>
#include <core/events.hpp>

struct event_handler_fixture
{
  std::shared_ptr<radix_relay_test::test_double_command_handler> test_cmd_handler;
  radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t queues{};
  radix_relay::core::event_handler<radix_relay_test::test_double_command_handler> handler;

  event_handler_fixture()
    : test_cmd_handler(std::make_shared<radix_relay_test::test_double_command_handler>()),
      handler(test_cmd_handler, queues)
  {}
};

TEST_CASE("Event handler processes simple raw commands", "[events][handler][raw_command]")
{
  const event_handler_fixture fixture;

  auto help_event = radix_relay::core::events::raw_command{ .input = "/help" };
  auto version_event = radix_relay::core::events::raw_command{ .input = "/version" };
  auto peers_event = radix_relay::core::events::raw_command{ .input = "/peers" };
  auto identities_event = radix_relay::core::events::raw_command{ .input = "/identities" };
  auto publish_event = radix_relay::core::events::raw_command{ .input = "/publish" };

  fixture.test_cmd_handler->clear_calls();

  fixture.handler.handle(help_event);
  CHECK(fixture.test_cmd_handler->was_called("help"));

  fixture.handler.handle(version_event);
  CHECK(fixture.test_cmd_handler->was_called("version"));

  fixture.handler.handle(peers_event);
  CHECK(fixture.test_cmd_handler->was_called("peers"));

  fixture.handler.handle(identities_event);
  CHECK(fixture.test_cmd_handler->was_called("identities"));

  fixture.handler.handle(publish_event);
  CHECK(fixture.test_cmd_handler->was_called("publish_identity"));

  CHECK(fixture.test_cmd_handler->get_call_count() == 5);
}

TEST_CASE("Event handler processes parameterized raw commands", "[events][handler][raw_command]")
{
  const event_handler_fixture fixture;

  auto mode_event = radix_relay::core::events::raw_command{ .input = "/mode internet" };
  auto send_event = radix_relay::core::events::raw_command{ .input = "/send alice hello" };
  auto broadcast_event = radix_relay::core::events::raw_command{ .input = "/broadcast test message" };
  auto connect_event = radix_relay::core::events::raw_command{ .input = "/connect wss://relay.example.com" };
  auto disconnect_event = radix_relay::core::events::raw_command{ .input = "/disconnect" };

  fixture.test_cmd_handler->clear_calls();

  fixture.handler.handle(mode_event);
  CHECK(fixture.test_cmd_handler->was_called("mode:internet"));

  fixture.handler.handle(send_event);
  CHECK(fixture.test_cmd_handler->was_called("send:alice:hello"));

  fixture.handler.handle(broadcast_event);
  CHECK(fixture.test_cmd_handler->was_called("broadcast:test message"));

  fixture.handler.handle(connect_event);
  CHECK(fixture.test_cmd_handler->was_called("connect:wss://relay.example.com"));

  fixture.handler.handle(disconnect_event);
  CHECK(fixture.test_cmd_handler->was_called("disconnect"));

  CHECK(fixture.test_cmd_handler->get_call_count() == 5);
}

TEST_CASE("Event handler processes unknown raw commands gracefully", "[events][handler][raw_command]")
{
  const event_handler_fixture fixture;

  auto unknown_event = radix_relay::core::events::raw_command{ .input = "unknown_command" };

  fixture.test_cmd_handler->clear_calls();

  REQUIRE_NOTHROW(fixture.handler.handle(unknown_event));
  CHECK(fixture.test_cmd_handler->get_call_count() == 0);
}

TEST_CASE("Event handler processes malformed raw commands gracefully", "[events][handler][raw_command]")
{
  const event_handler_fixture fixture;

  auto empty_event = radix_relay::core::events::raw_command{ .input = "" };
  auto incomplete_send = radix_relay::core::events::raw_command{ .input = "/send alice" };
  auto incomplete_mode = radix_relay::core::events::raw_command{ .input = "/mode" };

  fixture.test_cmd_handler->clear_calls();

  REQUIRE_NOTHROW(fixture.handler.handle(empty_event));
  REQUIRE_NOTHROW(fixture.handler.handle(incomplete_send));
  REQUIRE_NOTHROW(fixture.handler.handle(incomplete_mode));

  CHECK(fixture.test_cmd_handler->was_called("send::"));
}

TEST_CASE("Event handler processes chat context commands", "[events][handler][raw_command]")
{
  const event_handler_fixture fixture;

  auto chat_event = radix_relay::core::events::raw_command{ .input = "/chat alice" };
  auto leave_event = radix_relay::core::events::raw_command{ .input = "/leave" };

  fixture.test_cmd_handler->clear_calls();

  fixture.handler.handle(chat_event);
  CHECK(fixture.test_cmd_handler->was_called("chat:alice"));

  fixture.handler.handle(leave_event);
  CHECK(fixture.test_cmd_handler->was_called("leave"));

  CHECK(fixture.test_cmd_handler->get_call_count() == 2);
}

TEST_CASE("Event handler preprocesses plain text as send command in chat mode", "[events][handler][chat_mode]")
{
  auto test_cmd_handler = std::make_shared<radix_relay_test::test_double_command_handler>();
  test_cmd_handler->bridge->contacts.push_back(radix_relay::core::contact_info{ .rdx_fingerprint = "RDX:alice123",
    .nostr_pubkey = "npub_alice",
    .user_alias = "alice",
    .has_active_session = true });
  radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t const queues{};
  const radix_relay::core::event_handler handler{ test_cmd_handler, queues };

  handler.handle(radix_relay::core::events::raw_command{ .input = "/chat alice" });

  auto plain_text = radix_relay::core::events::raw_command{ .input = "hello" };
  handler.handle(plain_text);

  CHECK(test_cmd_handler->was_called("send:RDX:alice123:hello"));
}

TEST_CASE("Event handler does not preprocess slash commands in chat mode", "[events][handler][chat_mode]")
{
  auto test_cmd_handler = std::make_shared<radix_relay_test::test_double_command_handler>();
  test_cmd_handler->bridge->contacts.push_back(radix_relay::core::contact_info{ .rdx_fingerprint = "RDX:alice123",
    .nostr_pubkey = "npub_alice",
    .user_alias = "alice",
    .has_active_session = true });
  radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t const queues{};
  const radix_relay::core::event_handler handler{ test_cmd_handler, queues };

  handler.handle(radix_relay::core::events::raw_command{ .input = "/chat alice" });

  auto slash_command = radix_relay::core::events::raw_command{ .input = "/help" };
  handler.handle(slash_command);

  CHECK(test_cmd_handler->was_called("help"));
  CHECK_FALSE(test_cmd_handler->was_called("send"));
}

TEST_CASE("Event handler stops preprocessing after exiting chat mode", "[events][handler][chat_mode]")
{
  auto test_cmd_handler = std::make_shared<radix_relay_test::test_double_command_handler>();
  test_cmd_handler->bridge->contacts.push_back(radix_relay::core::contact_info{ .rdx_fingerprint = "RDX:alice123",
    .nostr_pubkey = "npub_alice",
    .user_alias = "alice",
    .has_active_session = true });
  radix_relay::core::event_handler<radix_relay_test::test_double_command_handler>::out_queues_t const queues{};
  const radix_relay::core::event_handler handler{ test_cmd_handler, queues };

  handler.handle(radix_relay::core::events::raw_command{ .input = "/chat alice" });
  handler.handle(radix_relay::core::events::raw_command{ .input = "/leave" });

  auto plain_text = radix_relay::core::events::raw_command{ .input = "hello" };
  handler.handle(plain_text);

  CHECK_FALSE(test_cmd_handler->was_called("send"));
}
