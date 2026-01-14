#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <tuple>

#include "test_doubles/test_double_signal_bridge.hpp"
#include <async/async_queue.hpp>
#include <core/command_handler.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>
#include <platform/env_utils.hpp>
#include <signal/signal_bridge.hpp>

template<class... Ts> struct overload : Ts...
{
  using Ts::operator()...;
};

struct command_handler_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>> display_out_queue;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::transport::in_t>> transport_out_queue;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::session_orchestrator::in_t>>
    session_out_queue;
  std::shared_ptr<radix_relay_test::test_double_signal_bridge> bridge;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::connection_monitor::in_t>>
    connection_monitor_queue;
  radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge> handler;

  command_handler_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      display_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>>(
          io_context)),
      transport_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::transport::in_t>>(io_context)),
      session_out_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::session_orchestrator::in_t>>(
          io_context)),
      bridge(std::make_shared<radix_relay_test::test_double_signal_bridge>()),
      connection_monitor_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::connection_monitor::in_t>>(
          io_context)),
      handler(bridge,
        radix_relay::core::command_handler<radix_relay_test::test_double_signal_bridge>::out_queues_t{
          .display = display_out_queue,
          .transport = transport_out_queue,
          .session = session_out_queue,
          .connection_monitor = connection_monitor_queue })
  {}

  [[nodiscard]] auto get_all_output() const -> std::string
  {
    std::string result;
    while (auto msg = display_out_queue->try_pop()) {
      std::visit(
        [&result](const auto &evt) {
          if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
            result += evt.message;
          }
        },
        *msg);
    }
    return result;
  }
};

TEST_CASE("help command emits display_message event with available commands", "[commands][handler][simple]")
{
  auto help_command = radix_relay::core::events::help{};
  const command_handler_fixture fixture;
  fixture.handler.handle(help_command);

  const auto output = fixture.get_all_output();
  CHECK(output.find("Interactive Commands") != std::string::npos);
}

TEST_CASE("version command outputs version information", "[commands][handler][simple]")
{
  auto version_command = radix_relay::core::events::version{};
  const command_handler_fixture fixture;
  fixture.handler.handle(version_command);
  CHECK(fixture.get_all_output().find("Radix Relay v") != std::string::npos);
}

TEST_CASE("peers command outputs peer discovery information", "[commands][handler][simple]")
{
  auto peers_command = radix_relay::core::events::peers{};
  const command_handler_fixture fixture;
  fixture.handler.handle(peers_command);
  CHECK(fixture.get_all_output().find("Connected Peers") != std::string::npos);
}

TEST_CASE("status command forwards query to connection monitor and outputs crypto status",
  "[commands][handler][simple]")
{
  auto status_command = radix_relay::core::events::status{};
  const command_handler_fixture fixture;
  fixture.handler.handle(status_command);

  auto monitor_event = fixture.connection_monitor_queue->try_pop();
  REQUIRE(monitor_event.has_value());
  if (monitor_event.has_value()) {
    CHECK(std::holds_alternative<radix_relay::core::events::connection_monitor::query_status>(*monitor_event));
  }

  const auto output = fixture.get_all_output();
  CHECK(output.find("Node Fingerprint") != std::string::npos);
  CHECK(output.find("RDX:") != std::string::npos);
}

TEST_CASE("sessions command with no sessions outputs no active sessions message", "[commands][handler][simple]")
{
  auto sessions_command = radix_relay::core::events::sessions{};
  const command_handler_fixture fixture;
  fixture.bridge->contacts_to_return = {};
  fixture.handler.handle(sessions_command);
  CHECK(fixture.get_all_output().find("No active sessions") != std::string::npos);
}

TEST_CASE("sessions command with established sessions outputs active sessions with contact information",
  "[commands][handler][simple]")
{
  auto sessions_command = radix_relay::core::events::sessions{};
  const command_handler_fixture fixture;
  fixture.bridge->contacts_to_return = {
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:alice123",
      .nostr_pubkey = "npub_alice",
      .user_alias = "Alice",
      .has_active_session = true,
    },
    radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:bob456",
      .nostr_pubkey = "npub_bob",
      .user_alias = "",
      .has_active_session = true,
    },
  };
  fixture.handler.handle(sessions_command);
  const auto output = fixture.get_all_output();
  CHECK(output.find("Active Sessions") != std::string::npos);
  CHECK(output.find("Alice") != std::string::npos);
  CHECK(output.find("RDX:alice123") != std::string::npos);
  CHECK(output.find("RDX:bob456") != std::string::npos);
}

TEST_CASE("scan command outputs scan information", "[commands][handler][simple]")
{
  auto scan_command = radix_relay::core::events::scan{};
  const command_handler_fixture fixture;
  fixture.handler.handle(scan_command);
  CHECK(fixture.get_all_output().find("Scanning") != std::string::npos);
}

TEST_CASE("identities command pushes list_identities event to session queue", "[commands][handler][simple]")
{
  auto identities_command = radix_relay::core::events::identities{};
  const command_handler_fixture fixture;
  fixture.handler.handle(identities_command);

  auto session_event = fixture.session_out_queue->try_pop();
  REQUIRE(session_event.has_value());
  if (session_event) { CHECK(std::holds_alternative<radix_relay::core::events::list_identities>(*session_event)); }
}

TEST_CASE("mode command outputs mode change confirmation", "[commands][handler][parameterized]")
{
  auto mode_command = radix_relay::core::events::mode{ .new_mode = "internet" };
  const command_handler_fixture fixture;
  fixture.handler.handle(mode_command);
  CHECK(fixture.get_all_output().find("internet") != std::string::npos);
}

TEST_CASE("send command pushes send event to session queue", "[commands][handler][parameterized]")
{
  auto send_command = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };
  const command_handler_fixture fixture;
  fixture.handler.handle(send_command);

  auto session_event = fixture.session_out_queue->try_pop();
  REQUIRE(session_event.has_value());
  if (session_event) {
    CHECK(std::holds_alternative<radix_relay::core::events::send>(*session_event));
    const auto &send_event = std::get<radix_relay::core::events::send>(*session_event);
    CHECK(send_event.peer == "alice");
    CHECK(send_event.message == "hello world");
  }
}

TEST_CASE("send command outputs send command confirmation with peer and message", "[commands][handler][parameterized]")
{
  auto send_command = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };
  const command_handler_fixture fixture;
  fixture.handler.handle(send_command);
  const auto output = fixture.get_all_output();
  CHECK(output.find("alice") != std::string::npos);
  CHECK(output.find("hello world") != std::string::npos);
}

TEST_CASE("broadcast command outputs broadcast command confirmation with message", "[commands][handler][parameterized]")
{
  auto broadcast_command = radix_relay::core::events::broadcast{ .message = "hello everyone" };
  const command_handler_fixture fixture;
  fixture.handler.handle(broadcast_command);
  CHECK(fixture.get_all_output().find("hello everyone") != std::string::npos);
}

TEST_CASE("connect command outputs connect command confirmation with relay URL", "[commands][handler][parameterized]")
{
  auto connect_command = radix_relay::core::events::connect{ .relay = "wss://relay.damus.io" };
  const command_handler_fixture fixture;
  fixture.handler.handle(connect_command);
  CHECK(fixture.get_all_output().find("relay.damus.io") != std::string::npos);
}

TEST_CASE("connect command pushes connect event to session queue", "[commands][handler][parameterized]")
{
  auto connect_command = radix_relay::core::events::connect{ .relay = "wss://relay.damus.io" };
  const command_handler_fixture fixture;
  fixture.handler.handle(connect_command);

  auto session_event = fixture.session_out_queue->try_pop();
  REQUIRE(session_event.has_value());
  if (session_event) {
    CHECK(std::holds_alternative<radix_relay::core::events::connect>(*session_event));
    const auto &connect_event = std::get<radix_relay::core::events::connect>(*session_event);
    CHECK(connect_event.relay == "wss://relay.damus.io");
  }
}

TEST_CASE("disconnect command outputs disconnect confirmation", "[commands][handler][parameterized]")
{
  auto disconnect_command = radix_relay::core::events::disconnect{};
  const command_handler_fixture fixture;
  fixture.handler.handle(disconnect_command);
  CHECK(fixture.get_all_output().find("Disconnecting") != std::string::npos);
}

TEST_CASE("disconnect command pushes transport disconnect event to transport queue",
  "[commands][handler][parameterized]")
{
  auto disconnect_command = radix_relay::core::events::disconnect{};
  const command_handler_fixture fixture;
  fixture.handler.handle(disconnect_command);

  auto transport_event = fixture.transport_out_queue->try_pop();
  REQUIRE(transport_event.has_value());
  if (transport_event) {
    CHECK(std::holds_alternative<radix_relay::core::events::transport::disconnect>(*transport_event));
  }
}

TEST_CASE("trust command pushes trust event to session queue", "[commands][handler][parameterized]")
{
  auto trust_command = radix_relay::core::events::trust{ .peer = "RDX:alice123", .alias = "Alice" };
  const command_handler_fixture fixture;
  fixture.handler.handle(trust_command);

  auto session_event = fixture.session_out_queue->try_pop();
  REQUIRE(session_event.has_value());
  if (session_event) {
    CHECK(std::holds_alternative<radix_relay::core::events::trust>(*session_event));
    const auto &trust = std::get<radix_relay::core::events::trust>(*session_event);
    CHECK(trust.peer == "RDX:alice123");
    CHECK(trust.alias == "Alice");
  }

  CHECK(fixture.get_all_output().find("RDX:alice123") != std::string::npos);
}

TEST_CASE("verify command outputs verify command confirmation with peer", "[commands][handler][parameterized]")
{
  auto verify_command = radix_relay::core::events::verify{ .peer = "bob" };
  const command_handler_fixture fixture;
  fixture.handler.handle(verify_command);
  CHECK(fixture.get_all_output().find("bob") != std::string::npos);
}

TEST_CASE("send command with empty parameters outputs usage information", "[commands][handler][validation]")
{
  auto send_command = radix_relay::core::events::send{ .peer = "", .message = "" };
  const command_handler_fixture fixture;
  fixture.handler.handle(send_command);
  CHECK(fixture.get_all_output().find("Usage") != std::string::npos);
}

TEST_CASE("broadcast command with empty message outputs usage information", "[commands][handler][validation]")
{
  auto broadcast_command = radix_relay::core::events::broadcast{ .message = "" };
  const command_handler_fixture fixture;
  fixture.handler.handle(broadcast_command);
  CHECK(fixture.get_all_output().find("Usage") != std::string::npos);
}

TEST_CASE("mode command with invalid mode outputs invalid mode error message", "[commands][handler][validation]")
{
  auto mode_command = radix_relay::core::events::mode{ .new_mode = "invalid" };
  const command_handler_fixture fixture;
  fixture.handler.handle(mode_command);
  CHECK(fixture.get_all_output().find("Invalid mode") != std::string::npos);
}

SCENARIO("Command handler processes chat context commands correctly", "[commands][handler][chat]")
{
  GIVEN("A command handler with test fixture")
  {
    WHEN("handling chat command with valid contact")
    {
      auto chat_command = radix_relay::core::events::chat{ .contact = "alice" };

      THEN("handler should enter chat mode and emit enter_chat_mode event")
      {
        const command_handler_fixture fixture;
        fixture.bridge->contacts_to_return.push_back(radix_relay::core::contact_info{
          .rdx_fingerprint = "RDX:alice123",
          .nostr_pubkey = "npub_alice",
          .user_alias = "alice",
          .has_active_session = true,
        });
        fixture.handler.handle(chat_command);

        bool found_enter_chat_mode = false;
        bool found_display_message = false;
        while (auto event = fixture.display_out_queue->try_pop()) {
          std::visit(overload{ [&found_enter_chat_mode](const radix_relay::core::events::enter_chat_mode & /*evt*/) {
                                found_enter_chat_mode = true;
                              },
                       [&found_display_message](const radix_relay::core::events::display_message &evt) {
                         found_display_message = evt.message.find("Entering chat with") != std::string::npos;
                       },
                       [](const auto & /*evt*/) {} },
            *event);
        }
        CHECK(found_enter_chat_mode);
        CHECK(found_display_message);
      }
    }

    WHEN("handling chat command with unknown contact")
    {
      auto chat_command = radix_relay::core::events::chat{ .contact = "unknown" };

      THEN("handler should emit error message")
      {
        const command_handler_fixture fixture;
        fixture.bridge->contacts_to_return.push_back(radix_relay::core::contact_info{
          .rdx_fingerprint = "RDX:alice123",
          .nostr_pubkey = "npub_alice",
          .user_alias = "alice",
          .has_active_session = true,
        });
        fixture.handler.handle(chat_command);

        const auto output = fixture.get_all_output();
        CHECK(output.find("Contact not found") != std::string::npos);
      }
    }

    WHEN("handling leave command")
    {
      auto leave_command = radix_relay::core::events::leave{};

      THEN("handler should exit chat mode and emit exit_chat_mode event")
      {
        const command_handler_fixture fixture;
        fixture.handler.handle(leave_command);

        bool found_exit_chat_mode = false;
        bool found_display_message = false;
        while (auto event = fixture.display_out_queue->try_pop()) {
          std::visit(overload{ [&found_exit_chat_mode](const radix_relay::core::events::exit_chat_mode & /*evt*/) {
                                found_exit_chat_mode = true;
                              },
                       [&found_display_message](const radix_relay::core::events::display_message &evt) {
                         found_display_message = evt.message.find("Exiting chat mode") != std::string::npos;
                       },
                       [](const auto & /*evt*/) {} },
            *event);
        }
        CHECK(found_exit_chat_mode);
        CHECK(found_display_message);
      }
    }
  }
}

SCENARIO("Command handler displays conversation history when entering chat", "[commands][handler][chat][history]")
{
  GIVEN("A contact with message history")
  {
    WHEN("entering chat with a contact that has messages")
    {
      auto chat_command = radix_relay::core::events::chat{ .contact = "alice" };

      THEN("handler should load and display conversation history")
      {
        const command_handler_fixture fixture;
        const std::string alice_rdx = "RDX:alice123";
        const auto conv_id = static_cast<std::int64_t>(std::hash<std::string>{}(alice_rdx) % 1000);

        constexpr std::uint64_t first_msg_time = 1000;
        constexpr std::uint64_t second_msg_time = 2000;
        constexpr std::uint64_t third_msg_time = 3000;

        fixture.bridge->contacts_to_return.push_back(radix_relay::core::contact_info{
          .rdx_fingerprint = alice_rdx,
          .nostr_pubkey = "npub_alice",
          .user_alias = "alice",
          .has_active_session = true,
        });

        fixture.bridge->messages_to_return = {
          radix_relay::signal::stored_message{ .id = 1,
            .conversation_id = conv_id,
            .direction = radix_relay::signal::MessageDirection::Outgoing,
            .timestamp = first_msg_time,
            .message_type = radix_relay::signal::MessageType::Text,
            .content = "Hello Alice",
            .delivery_status = radix_relay::signal::DeliveryStatus::Delivered,
            .was_prekey_message = false,
            .session_established = false },
          radix_relay::signal::stored_message{ .id = 2,
            .conversation_id = conv_id,
            .direction = radix_relay::signal::MessageDirection::Incoming,
            .timestamp = second_msg_time,
            .message_type = radix_relay::signal::MessageType::Text,
            .content = "Hi there",
            .delivery_status = radix_relay::signal::DeliveryStatus::Delivered,
            .was_prekey_message = false,
            .session_established = false },
          radix_relay::signal::stored_message{ .id = 3,
            .conversation_id = conv_id,
            .direction = radix_relay::signal::MessageDirection::Outgoing,
            .timestamp = third_msg_time,
            .message_type = radix_relay::signal::MessageType::Text,
            .content = "How are you?",
            .delivery_status = radix_relay::signal::DeliveryStatus::Delivered,
            .was_prekey_message = false,
            .session_established = false },
        };

        fixture.handler.handle(chat_command);

        CHECK(fixture.bridge->was_called("get_conversation_messages"));
        CHECK(fixture.bridge->was_called("mark_conversation_read_up_to"));
        CHECK(fixture.bridge->marked_read_rdx == alice_rdx);
        CHECK(fixture.bridge->marked_read_up_to_timestamp == third_msg_time);

        std::vector<radix_relay::core::events::display_message> history_messages;
        while (auto event = fixture.display_out_queue->try_pop()) {
          std::visit(
            [&history_messages](const auto &evt) {
              if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
                if (evt.contact_rdx.has_value() and evt.contact_rdx.value() == "RDX:alice123") {
                  history_messages.push_back(evt);
                }
              }
            },
            *event);
        }

        CHECK(history_messages.size() >= 3);

        bool found_outgoing = false;
        bool found_incoming = false;
        for (const auto &msg : history_messages) {
          if (msg.message.find("→ You:") != std::string::npos) { found_outgoing = true; }
          if (msg.message.find("← alice:") != std::string::npos) { found_incoming = true; }
        }
        CHECK(found_outgoing);
        CHECK(found_incoming);
      }
    }

    WHEN("entering chat with a contact that has no messages")
    {
      auto chat_command = radix_relay::core::events::chat{ .contact = "bob" };

      THEN("handler should still enter chat mode but display no history")
      {
        const command_handler_fixture fixture;
        fixture.bridge->contacts_to_return.push_back(radix_relay::core::contact_info{
          .rdx_fingerprint = "RDX:bob456",
          .nostr_pubkey = "npub_bob",
          .user_alias = "bob",
          .has_active_session = true,
        });

        fixture.bridge->messages_to_return = {};

        fixture.handler.handle(chat_command);

        CHECK(fixture.bridge->was_called("get_conversation_messages"));
        CHECK(fixture.bridge->was_called("mark_conversation_read"));

        bool found_enter_chat_mode = false;
        while (auto event = fixture.display_out_queue->try_pop()) {
          std::visit(overload{ [&found_enter_chat_mode](const radix_relay::core::events::enter_chat_mode & /*evt*/) {
                                found_enter_chat_mode = true;
                              },
                       [](const auto & /*evt*/) {} },
            *event);
        }
        CHECK(found_enter_chat_mode);
      }
    }
  }
}
