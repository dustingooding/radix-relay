#include "test_doubles/test_double_nostr_handler.hpp"
#include "test_doubles/test_double_nostr_transport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <radix_relay/nostr.hpp>
#include <ranges>

TEST_CASE("NostrDispatcher routes messages correctly", "[nostr][dispatcher]")
{
  SECTION("dispatch identity announcement event")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";
    auto e_data =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(pubkey, timestamp, "test_fingerprint");

    auto event = radix_relay::nostr::protocol::event::from_event_data(e_data);
    const auto event_str = event.serialize();

    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.size() == 1);
    CHECK(handler.encrypted_events.empty());
    CHECK(handler.session_events.empty());
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.empty());
    if (!handler.identity_events.empty()) { CHECK(handler.identity_events[0].pubkey == pubkey); }
  }

  SECTION("dispatch encrypted message event")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "recipient";
    auto e_data = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, "encrypted_payload", "session_id");
    e_data.pubkey = sender_pubkey;// Set for testing

    auto event = radix_relay::nostr::protocol::event::from_event_data(e_data);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.empty());
    CHECK(handler.encrypted_events.size() == 1);
    CHECK(handler.session_events.empty());
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.empty());
    CHECK(handler.encrypted_events[0].pubkey == sender_pubkey);
  }

  SECTION("dispatch session request event")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "recipient";
    auto e_data = radix_relay::nostr::protocol::event_data::create_session_request(
      sender_pubkey, timestamp, recipient_pubkey, "prekey_bundle");

    auto event = radix_relay::nostr::protocol::event::from_event_data(e_data);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.empty());
    CHECK(handler.encrypted_events.empty());
    CHECK(handler.session_events.size() == 1);
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.empty());
    CHECK(handler.session_events[0].pubkey == sender_pubkey);
  }

  SECTION("dispatch unknown message types to unknown handler")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";
    const radix_relay::nostr::protocol::event_data text_note_event{ .id = "",
      .pubkey = pubkey,
      .created_at = timestamp,
      .kind = static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::text_note),
      .tags = {},
      .content = "hello world",
      .sig = "" };

    auto event = radix_relay::nostr::protocol::event::from_event_data(text_note_event);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.empty());
    CHECK(handler.encrypted_events.empty());
    CHECK(handler.session_events.empty());
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.size() == 1);
    CHECK(handler.unknown_events[0].content == "hello world");
  }

  SECTION("dispatch bytes with valid JSON")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";
    auto e_data =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(pubkey, timestamp, "test_fingerprint");

    auto event = radix_relay::nostr::protocol::event::from_event_data(e_data);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.size() == 1);
    CHECK(handler.identity_events[0].pubkey == pubkey);
  }

  SECTION("dispatch bytes with invalid JSON does nothing")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const std::string invalid_json = "not valid json";
    const auto bytes = std::as_bytes(std::span(invalid_json));
    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.empty());
    CHECK(handler.encrypted_events.empty());
    CHECK(handler.session_events.empty());
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.empty());
  }

  SECTION("create transport callback")
  {
    radix_relay_test::TestDoubleNostrIncomingHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    auto callback = dispatcher.create_transport_callback();

    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";
    auto e_data =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(pubkey, timestamp, "test_fingerprint");

    auto event = radix_relay::nostr::protocol::event::from_event_data(e_data);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    callback(bytes);

    CHECK(handler.identity_events.size() == 1);
    CHECK(handler.identity_events[0].pubkey == pubkey);
  }
}

TEST_CASE("Outgoing protocol::event_data types work correctly", "[nostr][outgoing]")
{
  SECTION("outgoing identity announcement wraps protocol::event_data")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto base_event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);
    radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    CHECK(outgoing_event.pubkey == sender_pubkey);
    CHECK(outgoing_event.created_at == timestamp);
    CHECK(outgoing_event.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::identity_announcement));
    CHECK(outgoing_event.content == "radix_relay_node_v1");
    CHECK(outgoing_event.tags.size() == 3);
    CHECK(outgoing_event.tags[0] == std::vector<std::string>{ "signal_fingerprint", signal_fingerprint });
    CHECK(outgoing_event.tags[1] == std::vector<std::string>{ "radix_capabilities", "mesh,nostr" });
  }

  SECTION("outgoing encrypted message wraps protocol::event_data")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string encrypted_payload = "encrypted_signal_payload";
    const std::string session_id = "test_session_id";

    auto base_event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, encrypted_payload, session_id);
    base_event.pubkey = sender_pubkey;// Set for testing
    radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(base_event);

    CHECK(outgoing_event.pubkey == sender_pubkey);
    CHECK(outgoing_event.created_at == timestamp);
    CHECK(outgoing_event.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::encrypted_message));
    CHECK(outgoing_event.content == encrypted_payload);
    CHECK(outgoing_event.tags.size() == 3);
    CHECK(outgoing_event.tags[0] == std::vector<std::string>{ "p", recipient_pubkey });
    CHECK(outgoing_event.tags[1] == std::vector<std::string>{ "radix_peer", session_id });
  }

  SECTION("outgoing session request wraps protocol::event_data")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string prekey_bundle = "encoded_prekey_bundle";

    auto base_event = radix_relay::nostr::protocol::event_data::create_session_request(
      sender_pubkey, timestamp, recipient_pubkey, prekey_bundle);
    radix_relay::nostr::events::outgoing::session_request outgoing_event(base_event);

    CHECK(outgoing_event.pubkey == sender_pubkey);
    CHECK(outgoing_event.created_at == timestamp);
    CHECK(outgoing_event.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::session_request));
    CHECK(outgoing_event.content == prekey_bundle);
    CHECK(outgoing_event.tags.size() == 2);
    CHECK(outgoing_event.tags[0] == std::vector<std::string>{ "p", recipient_pubkey });
  }

  SECTION("outgoing events can be serialized")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto base_event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    auto json_str = outgoing_event.serialize();
    auto parsed = radix_relay::nostr::protocol::event_data::deserialize(json_str);

    REQUIRE(parsed.has_value());
    if (parsed.has_value()) {
      const auto &event = parsed.value();
      CHECK(event.pubkey == sender_pubkey);
      CHECK(event.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::identity_announcement));
      CHECK(event.content == "radix_relay_node_v1");
    }
  }

  SECTION("outgoing events preserve all protocol::event_data functionality")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto base_event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    CHECK(outgoing_event.is_radix_message());
    auto kind = outgoing_event.get_kind();
    REQUIRE(kind.has_value());
    if (kind.has_value()) { CHECK(kind.value() == radix_relay::nostr::protocol::kind::identity_announcement); }
  }
}


TEST_CASE("NostrOutgoingHandler sends events via transport", "[nostr][outgoing_handler]")
{
  SECTION("handle outgoing identity announcement")
  {
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::OutgoingHandler handler(transport);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto base_event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    handler.handle(outgoing_event);

    REQUIRE(transport.sent_messages.size() == 1);

    // Convert bytes to string to parse the protocol message
    std::string msg_str;
    msg_str.resize(transport.sent_messages[0].size());
    std::ranges::transform(
      transport.sent_messages[0], msg_str.begin(), [](std::byte byt) { return std::bit_cast<char>(byt); });

    // Parse as protocol::event message
    auto protocol_msg = radix_relay::nostr::protocol::event::deserialize(msg_str);
    REQUIRE(protocol_msg.has_value());
    if (protocol_msg.has_value()) {
      const auto &event = protocol_msg.value();
      CHECK(event.data.pubkey == sender_pubkey);
      CHECK(event.data.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::identity_announcement));
    }
  }

  SECTION("handle outgoing encrypted message")
  {
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::OutgoingHandler handler(transport);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string encrypted_payload = "encrypted_signal_payload";
    const std::string session_id = "test_session_id";

    auto base_event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, encrypted_payload, session_id);
    base_event.pubkey = sender_pubkey;// Set for testing
    const radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(base_event);

    handler.handle(outgoing_event);

    REQUIRE(transport.sent_messages.size() == 1);

    // Convert bytes to string to parse the protocol message
    std::string msg_str;
    msg_str.resize(transport.sent_messages[0].size());
    std::ranges::transform(
      transport.sent_messages[0], msg_str.begin(), [](std::byte byt) { return std::bit_cast<char>(byt); });

    // Parse as protocol::event message
    auto protocol_msg = radix_relay::nostr::protocol::event::deserialize(msg_str);
    REQUIRE(protocol_msg.has_value());
    if (protocol_msg.has_value()) {
      const auto &event = protocol_msg.value();
      CHECK(event.data.pubkey == sender_pubkey);
      CHECK(event.data.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::encrypted_message));
      CHECK(event.data.content == encrypted_payload);
    }
  }

  SECTION("handle outgoing session request")
  {
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::OutgoingHandler handler(transport);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string prekey_bundle = "encoded_prekey_bundle";

    auto base_event = radix_relay::nostr::protocol::event_data::create_session_request(
      sender_pubkey, timestamp, recipient_pubkey, prekey_bundle);
    const radix_relay::nostr::events::outgoing::session_request outgoing_event(base_event);

    handler.handle(outgoing_event);

    REQUIRE(transport.sent_messages.size() == 1);

    // Convert bytes to string to parse the protocol message
    std::string msg_str;
    msg_str.resize(transport.sent_messages[0].size());
    std::ranges::transform(
      transport.sent_messages[0], msg_str.begin(), [](std::byte byt) { return std::bit_cast<char>(byt); });

    // Parse as protocol::event message
    auto protocol_msg = radix_relay::nostr::protocol::event::deserialize(msg_str);
    REQUIRE(protocol_msg.has_value());
    if (protocol_msg.has_value()) {
      const auto &event = protocol_msg.value();
      CHECK(event.data.pubkey == sender_pubkey);
      CHECK(event.data.kind == static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::session_request));
      CHECK(event.data.content == prekey_bundle);
    }
  }
}
