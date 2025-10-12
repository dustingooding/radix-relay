#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/nostr_message_handler.hpp>
#include <signal_bridge_cxx/lib.h>

TEST_CASE("NostrMessageHandler handles incoming encrypted_message", "[nostr_message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_pure_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_pure_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = radix_relay::new_signal_bridge(alice_path.c_str());
    auto bob_bridge = radix_relay::new_signal_bridge(bob_path.c_str());

    auto alice_bundle_json = radix_relay::generate_prekey_bundle_announcement(*alice_bridge, "test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();
    auto alice_rdx =
      radix_relay::add_contact_and_establish_session_from_base64(*bob_bridge, alice_bundle_base64.c_str(), "alice");

    auto bob_bundle_json = radix_relay::generate_prekey_bundle_announcement(*bob_bridge, "test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();
    auto bob_rdx =
      radix_relay::add_contact_and_establish_session_from_base64(*alice_bridge, bob_bundle_base64.c_str(), "bob");

    const std::string plaintext = "Hello Bob!";
    std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
    auto encrypted_bytes = radix_relay::encrypt_message(*alice_bridge,
      std::string(bob_rdx).c_str(),
      rust::Slice<const uint8_t>{ message_bytes.data(), message_bytes.size() });

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.id = "test_event_id";
    event_data.pubkey = "alice_pubkey";
    event_data.created_at = test_timestamp;
    event_data.kind = static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::encrypted_message);
    event_data.content = hex_content;
    event_data.sig = "signature";
    event_data.tags.push_back({ "p", std::string(alice_rdx) });

    const radix_relay::nostr::events::incoming::encrypted_message event{ event_data };

    radix_relay::NostrMessageHandler handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->sender_rdx == std::string(alice_rdx));
      CHECK(result->content == plaintext);
      CHECK(result->timestamp == test_timestamp);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("NostrMessageHandler handles incoming bundle_announcement", "[nostr_message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_bundle_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_bundle_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = radix_relay::new_signal_bridge(alice_path.c_str());
    auto bob_bridge = radix_relay::new_signal_bridge(bob_path.c_str());

    auto alice_bundle_json = radix_relay::generate_prekey_bundle_announcement(*alice_bridge, "test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.pubkey = "alice_nostr_pubkey";
    event_data.kind = static_cast<std::uint32_t>(radix_relay::nostr::protocol::kind::bundle_announcement);
    event_data.content = alice_bundle_base64;
    event_data.created_at = test_timestamp;
    event_data.id = "bundle_event_id";
    event_data.sig = "bundle_signature";

    const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };

    radix_relay::NostrMessageHandler handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->peer_rdx.starts_with("RDX:"));
      CHECK(result->peer_rdx.length() == 68);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("NostrMessageHandler handles send command", "[nostr_message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_send_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_send_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = radix_relay::new_signal_bridge(alice_path.c_str());
    auto bob_bridge = radix_relay::new_signal_bridge(bob_path.c_str());

    auto bob_bundle_json = radix_relay::generate_prekey_bundle_announcement(*bob_bridge, "test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();

    auto bob_rdx =
      radix_relay::add_contact_and_establish_session_from_base64(*alice_bridge, bob_bundle_base64.c_str(), "bob");

    const std::string plaintext = "Hello Bob!";

    radix_relay::NostrMessageHandler handler(alice_bridge);
    auto [event_id, bytes] =
      handler.handle(radix_relay::events::send{ .peer = std::string(bob_rdx), .message = plaintext });

    CHECK_FALSE(event_id.empty());

    std::string json_str;
    json_str.reserve(bytes.size());
    std::ranges::transform(bytes, std::back_inserter(json_str), [](std::byte byte) { return static_cast<char>(byte); });
    auto parsed = nlohmann::json::parse(json_str);

    CHECK(parsed.is_array());
    CHECK(parsed.size() == 2);
    CHECK(parsed[0] == "EVENT");
    CHECK(parsed[1].contains("kind"));
    CHECK(parsed[1]["kind"] == 40001);
    CHECK(parsed[1].contains("content"));
    CHECK_FALSE(parsed[1]["content"].get<std::string>().empty());
    CHECK(parsed[1].contains("tags"));

    const auto &tags = parsed[1]["tags"];
    const bool found_p_tag =
      std::ranges::any_of(tags, [](const auto &tag) { return tag.size() >= 2 and tag[0] == "p"; });
    const bool found_peer_tag =
      std::ranges::any_of(tags, [](const auto &tag) { return tag.size() >= 2 and tag[0] == "radix_peer"; });
    const bool found_version_tag =
      std::ranges::any_of(tags, [](const auto &tag) { return tag.size() >= 2 and tag[0] == "radix_version"; });
    CHECK(found_p_tag);
    CHECK(found_peer_tag);
    CHECK(found_version_tag);
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("NostrMessageHandler handles publish_identity command", "[nostr_message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_publish_alice.db";
  std::filesystem::remove(alice_path);

  {
    auto alice_bridge = radix_relay::new_signal_bridge(alice_path.c_str());

    radix_relay::NostrMessageHandler handler(alice_bridge);
    auto [event_id, bytes] = handler.handle(radix_relay::events::publish_identity{});

    CHECK_FALSE(event_id.empty());

    std::string json_str;
    json_str.reserve(bytes.size());
    std::ranges::transform(bytes, std::back_inserter(json_str), [](std::byte byte) { return static_cast<char>(byte); });
    auto parsed = nlohmann::json::parse(json_str);

    CHECK(parsed.is_array());
    CHECK(parsed.size() == 2);
    CHECK(parsed[0] == "EVENT");
    CHECK(parsed[1].contains("kind"));
    CHECK(parsed[1]["kind"] == 30078);
    CHECK(parsed[1].contains("content"));
    CHECK_FALSE(parsed[1]["content"].get<std::string>().empty());
  }

  std::filesystem::remove(alice_path);
}

TEST_CASE("NostrMessageHandler handles trust command", "[nostr_message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_trust_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_trust_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = radix_relay::new_signal_bridge(alice_path.c_str());
    auto bob_bridge = radix_relay::new_signal_bridge(bob_path.c_str());

    auto bob_bundle_json = radix_relay::generate_prekey_bundle_announcement(*bob_bridge, "test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();

    auto bob_rdx =
      radix_relay::add_contact_and_establish_session_from_base64(*alice_bridge, bob_bundle_base64.c_str(), "");

    const std::string alias = "bob_alias";

    radix_relay::NostrMessageHandler handler(alice_bridge);
    handler.handle(radix_relay::events::trust{ .peer = std::string(bob_rdx), .alias = alias });

    auto contact = radix_relay::lookup_contact(*alice_bridge, alias.c_str());
    CHECK(std::string(contact.rdx_fingerprint) == std::string(bob_rdx));
    CHECK(std::string(contact.user_alias) == alias);
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}
