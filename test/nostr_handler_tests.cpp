#include "test_doubles/test_double_nostr_handler.hpp"
#include "test_doubles/test_double_nostr_transport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <radix_relay/nostr.hpp>
#include <ranges>

TEST_CASE("NostrDispatcher routes messages correctly", "[nostr][dispatcher]")
{
  SECTION("dispatch identity announcement event")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
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
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "recipient";
    auto e_data = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, "encrypted_payload", "session_id");
    e_data.pubkey = sender_pubkey;

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
    radix_relay_test::TestDoubleNostrHandler handler;
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

  SECTION("dispatch OK message")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const std::string event_id = "test_event_id_12345";
    const std::string ok_json = R"(["OK",")" + event_id + R"(",true,""])";
    const auto bytes = std::as_bytes(std::span(ok_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.ok_msgs.size() == 1);
    if (!handler.ok_msgs.empty()) {
      CHECK(handler.ok_msgs[0].event_id == event_id);
      CHECK(handler.ok_msgs[0].accepted == true);
    }
  }

  SECTION("dispatch EOSE message")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const std::string subscription_id = "test_sub_123";
    const std::string eose_json = R"(["EOSE",")" + subscription_id + R"("])";
    const auto bytes = std::as_bytes(std::span(eose_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.eose_msgs.size() == 1);
    if (!handler.eose_msgs.empty()) { CHECK(handler.eose_msgs[0].subscription_id == subscription_id); }
  }

  SECTION("dispatch unknown message types to unknown handler")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
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

  SECTION("dispatch unknown protocol message")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const std::string unknown_protocol = R"(["NOTICE","This is a relay notice"])";
    const auto bytes = std::as_bytes(std::span(unknown_protocol));
    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.unknown_msgs.size() == 1);
    if (!handler.unknown_msgs.empty()) { CHECK(handler.unknown_msgs[0] == unknown_protocol); }
  }

  SECTION("dispatch bytes with invalid JSON does nothing")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler);

    const std::string invalid_json = "not valid json";
    const auto bytes = std::as_bytes(std::span(invalid_json));
    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.identity_events.empty());
    CHECK(handler.encrypted_events.empty());
    CHECK(handler.session_events.empty());
    CHECK(handler.status_events.empty());
    CHECK(handler.unknown_events.empty());
    CHECK(handler.unknown_msgs.size() == 1);
  }

  SECTION("create transport callback")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
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


TEST_CASE("Session forwards outgoing events to handler", "[nostr][session][outgoing]")
{
  SECTION("handle outgoing identity announcement")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto base_event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    session.handle(outgoing_event);

    REQUIRE(handler.outgoing_identity_events.size() == 1);
    CHECK(handler.outgoing_identity_events[0].pubkey == sender_pubkey);
  }

  SECTION("handle outgoing encrypted message")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string encrypted_payload = "encrypted_signal_payload";
    const std::string session_id = "test_session_id";

    auto base_event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, encrypted_payload, session_id);
    base_event.pubkey = sender_pubkey;
    const radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(base_event);

    session.handle(outgoing_event);

    REQUIRE(handler.outgoing_encrypted_events.size() == 1);
    CHECK(handler.outgoing_encrypted_events[0].pubkey == sender_pubkey);
  }

  SECTION("handle outgoing session request")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string prekey_bundle = "encoded_prekey_bundle";

    auto base_event = radix_relay::nostr::protocol::event_data::create_session_request(
      sender_pubkey, timestamp, recipient_pubkey, prekey_bundle);
    const radix_relay::nostr::events::outgoing::session_request outgoing_event(base_event);

    session.handle(outgoing_event);

    REQUIRE(handler.outgoing_session_events.size() == 1);
    CHECK(handler.outgoing_session_events[0].pubkey == sender_pubkey);
  }

  SECTION("handle outgoing plaintext message")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const radix_relay::nostr::events::outgoing::plaintext_message plaintext_event{ "recipient", "test message" };

    session.handle(plaintext_event);

    REQUIRE(handler.plaintext_messages.size() == 1);
    CHECK(handler.plaintext_messages[0].recipient == "recipient");
    CHECK(handler.plaintext_messages[0].message == "test message");
  }

  SECTION("handle outgoing subscription request")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const std::string subscription_json = R"(["REQ","sub_id",{"kinds":[40001]}])";
    const radix_relay::nostr::events::outgoing::subscription_request sub_event{ subscription_json };

    session.handle(sub_event);

    REQUIRE(handler.subscription_requests.size() == 1);
    CHECK(handler.subscription_requests[0].subscription_json == subscription_json);
  }
}

TEST_CASE("Session provides unified TX+RX interface", "[nostr][session][unified]")
{
  SECTION("session receives incoming messages via dispatcher")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    const radix_relay::nostr::Session session(transport, handler);

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

    if (transport.message_callback) { transport.message_callback(bytes); }

    CHECK(handler.identity_events.size() == 1);
    if (!handler.identity_events.empty()) { CHECK(handler.identity_events[0].pubkey == pubkey); }
  }

  SECTION("session handles bidirectional communication")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";

    auto outgoing_base = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, "sender_fingerprint");
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(outgoing_base);
    session.handle(outgoing_event);

    auto incoming_base = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      recipient_pubkey, timestamp, "recipient_fingerprint");
    auto event = radix_relay::nostr::protocol::event::from_event_data(incoming_base);
    const auto event_str = event.serialize();
    std::vector<std::byte> bytes;
    bytes.resize(event_str.size());
    std::ranges::transform(
      event_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
    if (transport.message_callback) { transport.message_callback(bytes); }

    CHECK(handler.outgoing_identity_events.size() == 1);
    CHECK(handler.identity_events.size() == 1);
  }

  SECTION("session handle method forwards to handler")
  {
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport;
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string encrypted_payload = "encrypted_data";
    const std::string session_id = "session_123";

    auto base_event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, encrypted_payload, session_id);
    base_event.pubkey = sender_pubkey;
    const radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(base_event);

    session.handle(outgoing_event);

    CHECK(handler.outgoing_encrypted_events.size() == 1);
  }
}
