#include "internal_use_only/config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nostr/protocol.hpp>
#include <ranges>

// Helper function to convert string literals to byte spans for testing
auto string_to_bytes(const std::string &str) -> std::vector<std::byte>
{
  std::vector<std::byte> bytes;
  bytes.resize(str.size());
  std::ranges::transform(
    str, bytes.begin(), [](char character) -> std::byte { return std::bit_cast<std::byte>(character); });
  return bytes;
}

TEST_CASE("protocol::event_data can be parsed from JSON", "[nostr][parse]")
{
  SECTION("parse valid Nostr event")
  {
    const auto test_timestamp = 1234567890U;
    const auto radix_kind = radix_relay::nostr::protocol::kind::encrypted_message;
    const std::string json_event = R"({
      "id": "a1b2c3d4e5f6789012345678901234567890123456789012345678901234567890",
      "pubkey": "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
      "created_at": 1234567890,
      "kind": 40001,
      "tags": [
        ["p", "recipient_pubkey"]
      ],
      "content": "encrypted_signal_payload",
      "sig": "signature_hex"
    })";

    auto event = radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(json_event));

    REQUIRE(event.has_value());
    if (event.has_value()) {
      const auto &evt = event.value();
      CHECK(evt.id == "a1b2c3d4e5f6789012345678901234567890123456789012345678901234567890");
      CHECK(evt.pubkey == "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
      CHECK(evt.created_at == test_timestamp);
      CHECK(evt.kind == radix_kind);
      CHECK(evt.content == "encrypted_signal_payload");
      CHECK(evt.sig == "signature_hex");
      REQUIRE(evt.tags.size() == 1);
      CHECK(evt.tags[0] == std::vector<std::string>{ "p", "recipient_pubkey" });
    }
  }

  SECTION("parse event with empty tags")
  {
    const std::string json_event = R"({
      "id": "test_id",
      "pubkey": "test_pubkey",
      "created_at": 1234567890,
      "kind": 1,
      "tags": [],
      "content": "test content",
      "sig": "test_sig"
    })";

    auto event = radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(json_event));

    REQUIRE(event.has_value());
    if (event.has_value()) {
      const auto &evt = event.value();
      CHECK(evt.tags.empty());
    }
  }

  SECTION("reject invalid JSON")
  {
    const std::string invalid_json = "not valid json";
    auto event = radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(invalid_json));
    CHECK_FALSE(event.has_value());
  }

  SECTION("reject missing required fields")
  {
    const std::string missing_id = R"({
      "pubkey": "test_pubkey",
      "created_at": 1234567890,
      "kind": 1,
      "tags": [],
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(missing_id)).has_value());

    const std::string missing_pubkey = R"({
      "id": "test_id",
      "created_at": 1234567890,
      "kind": 1,
      "tags": [],
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(missing_pubkey)).has_value());

    const std::string invalid_created_at = R"({
      "id": "test_id",
      "pubkey": "test_pubkey",
      "created_at": "not_a_number",
      "kind": 1,
      "tags": [],
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(invalid_created_at)).has_value());
  }

  SECTION("reject malformed tags")
  {
    const std::string invalid_tags = R"({
      "id": "test_id",
      "pubkey": "test_pubkey",
      "created_at": 1234567890,
      "kind": 1,
      "tags": "not_an_array",
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(invalid_tags)).has_value());

    const std::string invalid_tag_item = R"({
      "id": "test_id",
      "pubkey": "test_pubkey",
      "created_at": 1234567890,
      "kind": 1,
      "tags": ["not_an_array"],
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(invalid_tag_item)).has_value());

    const std::string invalid_tag_element = R"({
      "id": "test_id",
      "pubkey": "test_pubkey",
      "created_at": 1234567890,
      "kind": 1,
      "tags": [["p", 123]],
      "content": "test",
      "sig": "test_sig"
    })";
    CHECK_FALSE(
      radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(invalid_tag_element)).has_value());
  }
}

TEST_CASE("protocol::event_data can be serialized to JSON", "[nostr][serialize]")
{
  SECTION("serialize complete event")
  {
    const auto test_timestamp = 1234567890U;
    const auto radix_kind = radix_relay::nostr::protocol::kind::encrypted_message;
    const radix_relay::nostr::protocol::event_data event{ .id = "test_id",
      .pubkey = "test_pubkey",
      .created_at = test_timestamp,
      .kind = radix_kind,
      .tags = { { "p", "recipient" } },
      .content = "test content",
      .sig = "test_signature" };

    auto json_str = event.serialize();
    auto parsed = radix_relay::nostr::protocol::event_data::deserialize(json_str);

    REQUIRE(parsed.has_value());
    if (parsed.has_value()) {
      const auto &evt = parsed.value();
      CHECK(evt.id == event.id);
      CHECK(evt.pubkey == event.pubkey);
      CHECK(evt.created_at == event.created_at);
      CHECK(evt.kind == event.kind);
      CHECK(evt.tags == event.tags);
      CHECK(evt.content == event.content);
      CHECK(evt.sig == event.sig);
    }
  }

  SECTION("serialize event with empty tags")
  {
    const auto test_timestamp = 1234567890U;
    const radix_relay::nostr::protocol::event_data event{ .id = "test_id",
      .pubkey = "test_pubkey",
      .created_at = test_timestamp,
      .kind = radix_relay::nostr::protocol::kind::profile_metadata,
      .tags = {},
      .content = "test content",
      .sig = "test_signature" };

    auto json_str = event.serialize();
    auto parsed = radix_relay::nostr::protocol::event_data::deserialize(json_str);

    REQUIRE(parsed.has_value());
    if (parsed.has_value()) {
      const auto &evt = parsed.value();
      CHECK(evt.tags.empty());
    }
  }
}

TEST_CASE("protocol::event_data round-trip serialization", "[nostr][roundtrip]")
{
  SECTION("complex event with multiple tags")
  {
    const auto test_timestamp = 1234567890U;
    const auto radix_kind = radix_relay::nostr::protocol::kind::encrypted_message;
    const radix_relay::nostr::protocol::event_data original{
      .id = "a1b2c3d4e5f6789012345678901234567890123456789012345678901234567890",
      .pubkey = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
      .created_at = test_timestamp,
      .kind = radix_kind,
      .tags = { { "p", "recipient_pubkey_1" },
        { "p", "recipient_pubkey_2" },
        { "radix_version", "1.0" },
        { "e", "referenced_event_id" } },
      .content = "base64_encoded_signal_ciphertext",
      .sig = "64byte_schnorr_signature_hex"
    };

    auto serialized = original.serialize();
    auto deserialized = radix_relay::nostr::protocol::event_data::deserialize(serialized);

    REQUIRE(deserialized.has_value());
    if (deserialized.has_value()) {
      const auto &evt = deserialized.value();
      CHECK(evt.id == original.id);
      CHECK(evt.pubkey == original.pubkey);
      CHECK(evt.created_at == original.created_at);
      CHECK(evt.kind == original.kind);
      CHECK(evt.tags == original.tags);
      CHECK(evt.content == original.content);
      CHECK(evt.sig == original.sig);
    }
  }
}

TEST_CASE("protocol::event_data factory methods create correct message types", "[nostr][factory]")
{
  SECTION("create identity announcement message")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string signal_fingerprint = "test_signal_fingerprint";

    auto event = radix_relay::nostr::protocol::event_data::create_identity_announcement(
      sender_pubkey, timestamp, signal_fingerprint);

    CHECK(event.pubkey == sender_pubkey);
    CHECK(event.created_at == timestamp);
    CHECK(event.kind == radix_relay::nostr::protocol::kind::identity_announcement);
    CHECK(event.content == "radix_relay_node_v1");
    REQUIRE(event.tags.size() == 3);
    CHECK(event.tags[0] == std::vector<std::string>{ "signal_fingerprint", signal_fingerprint });
    CHECK(event.tags[1] == std::vector<std::string>{ "radix_capabilities", "mesh,nostr" });
    CHECK(
      event.tags[2] == std::vector<std::string>{ "radix_version", std::string{ radix_relay::cmake::project_version } });
  }

  SECTION("create encrypted message")
  {
    const auto timestamp = 1234567890U;
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string encrypted_payload = "encrypted_signal_payload";

    auto event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
      timestamp, recipient_pubkey, encrypted_payload);

    CHECK(event.pubkey.empty());// Not provided to create_encrypted_message
    CHECK(event.created_at == timestamp);
    CHECK(event.kind == radix_relay::nostr::protocol::kind::encrypted_message);
    CHECK(event.content == encrypted_payload);
    REQUIRE(event.tags.size() == 2);
    CHECK(event.tags[0] == std::vector<std::string>{ "p", recipient_pubkey });
    CHECK(
      event.tags[1] == std::vector<std::string>{ "radix_version", std::string{ radix_relay::cmake::project_version } });
  }

  SECTION("create session request message")
  {
    const auto timestamp = 1234567890U;
    const std::string sender_pubkey = "test_sender_pubkey";
    const std::string recipient_pubkey = "test_recipient_pubkey";
    const std::string prekey_bundle = "encoded_prekey_bundle";

    auto event = radix_relay::nostr::protocol::event_data::create_session_request(
      sender_pubkey, timestamp, recipient_pubkey, prekey_bundle);

    CHECK(event.pubkey == sender_pubkey);
    CHECK(event.created_at == timestamp);
    CHECK(event.kind == radix_relay::nostr::protocol::kind::session_request);
    CHECK(event.content == prekey_bundle);
    REQUIRE(event.tags.size() == 2);
    CHECK(event.tags[0] == std::vector<std::string>{ "p", recipient_pubkey });
    CHECK(
      event.tags[1] == std::vector<std::string>{ "radix_version", std::string{ radix_relay::cmake::project_version } });
  }
}

TEST_CASE("protocol::event_data helper methods work correctly", "[nostr][helpers]")
{
  SECTION("is_radix_message identifies Radix events correctly")
  {
    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";

    const radix_relay::nostr::protocol::event_data standard_event{ .id = "",
      .pubkey = pubkey,
      .created_at = timestamp,
      .kind = radix_relay::nostr::protocol::kind::text_note,
      .tags = {},
      .content = "hello world",
      .sig = "" };
    CHECK_FALSE(standard_event.is_radix_message());

    auto radix_event =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(pubkey, timestamp, "test_fingerprint");
    CHECK(radix_event.is_radix_message());
  }

  SECTION("get_kind returns correct enum values")
  {
    const auto timestamp = 1234567890U;
    const std::string pubkey = "test_pubkey";

    auto identity_event =
      radix_relay::nostr::protocol::event_data::create_identity_announcement(pubkey, timestamp, "test_fingerprint");
    auto kind = identity_event.get_kind();
    REQUIRE(kind.has_value());
    if (kind.has_value()) { CHECK(kind.value() == radix_relay::nostr::protocol::kind::identity_announcement); }

    const auto unknown_kind = static_cast<radix_relay::nostr::protocol::kind>(65534);
    const radix_relay::nostr::protocol::event_data unknown_event{ .id = "",
      .pubkey = pubkey,
      .created_at = timestamp,
      .kind = unknown_kind,
      .tags = {},
      .content = "unknown",
      .sig = "" };
    CHECK_FALSE(unknown_event.get_kind().has_value());
  }
}
