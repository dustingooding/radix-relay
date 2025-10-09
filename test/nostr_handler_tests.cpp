#include "test_doubles/test_double_nostr_handler.hpp"
#include "test_doubles/test_double_nostr_transport.hpp"
#include "test_doubles/test_double_request_tracker.hpp"
#include <catch2/catch_test_macros.hpp>
#include <radix_relay/nostr.hpp>
#include <radix_relay/nostr_request_tracker.hpp>
#include <ranges>

TEST_CASE("NostrDispatcher routes messages correctly", "[nostr][dispatcher]")
{
  SECTION("dispatch identity announcement event")
  {
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

    const std::string subscription_id = "test_sub_123";
    const std::string eose_json = R"(["EOSE",")" + subscription_id + R"("])";
    const auto bytes = std::as_bytes(std::span(eose_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.eose_msgs.size() == 1);
    if (!handler.eose_msgs.empty()) { CHECK(handler.eose_msgs[0].subscription_id == subscription_id); }
  }

  SECTION("dispatch unknown message types to unknown handler")
  {
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

    const std::string unknown_protocol = R"(["NOTICE","This is a relay notice"])";
    const auto bytes = std::as_bytes(std::span(unknown_protocol));
    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.unknown_msgs.size() == 1);
    if (!handler.unknown_msgs.empty()) { CHECK(handler.unknown_msgs[0] == unknown_protocol); }
  }

  SECTION("dispatch bytes with invalid JSON does nothing")
  {
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    radix_relay_test::TestDoubleRequestTracker tracker;

    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher<radix_relay_test::TestDoubleNostrHandler, radix_relay_test::TestDoubleRequestTracker>
      dispatcher(handler, tracker);

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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
    radix_relay::nostr::Session session(transport, handler);

    const radix_relay::nostr::events::outgoing::plaintext_message plaintext_event{ "recipient", "test message" };

    session.handle(plaintext_event);

    REQUIRE(handler.plaintext_messages.size() == 1);
    CHECK(handler.plaintext_messages[0].recipient == "recipient");
    CHECK(handler.plaintext_messages[0].message == "test message");
  }

  SECTION("handle outgoing subscription request")
  {
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
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

TEST_CASE("Dispatcher integrates with RequestTracker", "[nostr][dispatcher][request_tracker]")
{
  SECTION("dispatcher accepts RequestTracker reference in constructor")
  {
    boost::asio::io_context io_context;
    radix_relay::nostr::RequestTracker tracker(io_context);
    radix_relay_test::TestDoubleNostrHandler handler;
    const radix_relay::nostr::Dispatcher dispatcher(handler, tracker);

    CHECK(true);
  }

  SECTION("dispatcher resolves tracker when OK message arrives")
  {
    boost::asio::io_context io_context;
    radix_relay::nostr::RequestTracker tracker(io_context);
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler, tracker);

    bool callback_invoked = false;
    radix_relay::nostr::protocol::ok received_response;

    auto callback = [&callback_invoked, &received_response](const radix_relay::nostr::protocol::ok &response) {
      callback_invoked = true;
      received_response = response;
    };

    constexpr auto timeout = std::chrono::seconds(5);
    const std::string event_id = "test_event_id_67890";
    tracker.track(event_id, callback, timeout);

    const std::string ok_json = R"(["OK",")" + event_id + R"(",true,""])";
    const auto bytes = std::as_bytes(std::span(ok_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(callback_invoked);
    CHECK(received_response.event_id == event_id);
    CHECK(received_response.accepted == true);
  }

  SECTION("dispatcher still calls handler after resolving tracker")
  {
    boost::asio::io_context io_context;
    radix_relay::nostr::RequestTracker tracker(io_context);
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler, tracker);

    bool callback_invoked = false;
    auto callback = [&callback_invoked](const radix_relay::nostr::protocol::ok &) { callback_invoked = true; };

    constexpr auto timeout = std::chrono::seconds(5);
    const std::string event_id = "test_event_id_99999";
    tracker.track(event_id, callback, timeout);

    const std::string ok_json = R"(["OK",")" + event_id + R"(",true,""])";
    const auto bytes = std::as_bytes(std::span(ok_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(callback_invoked);
    CHECK(handler.ok_msgs.size() == 1);
    CHECK(handler.ok_msgs[0].event_id == event_id);
  }

  SECTION("dispatcher handles OK without tracked request")
  {
    boost::asio::io_context io_context;
    radix_relay::nostr::RequestTracker tracker(io_context);
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay::nostr::Dispatcher dispatcher(handler, tracker);

    const std::string event_id = "untracked_event";
    const std::string ok_json = R"(["OK",")" + event_id + R"(",false,"duplicate"])";
    const auto bytes = std::as_bytes(std::span(ok_json));

    dispatcher.dispatch_bytes(bytes);

    CHECK(handler.ok_msgs.size() == 1);
    CHECK(handler.ok_msgs[0].event_id == event_id);
    CHECK(handler.ok_msgs[0].accepted == false);
  }
}

TEST_CASE("Session integrates with RequestTracker", "[nostr][session][request_tracker]")
{
  SECTION("session owns RequestTracker instance")
  {
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
    const radix_relay::nostr::Session session(transport, handler);

    CHECK(true);
  }

  SECTION("Session handle() forwards events to the handler")
  {
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
    radix_relay::nostr::Session session(transport, handler);

    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender";
    auto base_event =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(sender_pubkey, timestamp, "fingerprint");
    const radix_relay::nostr::events::outgoing::identity_announcement outgoing_event(base_event);

    session.handle(outgoing_event);

    CHECK(handler.outgoing_identity_events.size() == 1);
  }

  SECTION("Session handle() awaitable overload tracks and returns OK")
  {
    boost::asio::io_context io_context;
    radix_relay_test::TestDoubleNostrHandler handler;
    radix_relay_test::TestDoubleNostrTransport transport(io_context);
    radix_relay::nostr::Session session(transport, handler);

    auto result = std::make_shared<radix_relay::nostr::protocol::ok>();
    auto completed = std::make_shared<bool>(false);

    const auto timestamp = 1234567890U;
    auto base_event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, "recipient", "payload", "session_id");
    base_event.id = "async_event_id";
    const radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(base_event);

    auto user_coroutine = [](
                            std::reference_wrapper<radix_relay::nostr::Session<radix_relay_test::TestDoubleNostrHandler,
                              radix_relay_test::TestDoubleNostrTransport>> session_ref,
                            radix_relay::nostr::events::outgoing::encrypted_message event,
                            std::shared_ptr<radix_relay::nostr::protocol::ok> result_ptr,
                            std::shared_ptr<bool> completed_ptr) -> boost::asio::awaitable<void> {
      constexpr auto timeout = std::chrono::seconds(5);
      *result_ptr = co_await session_ref.get().handle(event, timeout);
      *completed_ptr = true;
    };

    boost::asio::co_spawn(
      io_context, user_coroutine(std::ref(session), outgoing_event, result, completed), boost::asio::detached);

    boost::asio::post(io_context, [&transport, &base_event]() {
      const std::string ok_json = R"(["OK",")" + base_event.id + R"(",true,"accepted"])";
      const auto bytes = std::as_bytes(std::span(ok_json));
      if (transport.message_callback) { transport.message_callback(bytes); }
    });

    io_context.run();

    CHECK(*completed);
    CHECK(result->event_id == "async_event_id");
    CHECK(result->accepted);
    CHECK(result->message == "accepted");
  }
}
