#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <nostr/message_handler.hpp>
#include <signal/signal_bridge.hpp>

TEST_CASE("message_handler handles incoming encrypted_message", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_pure_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_pure_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();
    auto alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();
    auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    const std::string plaintext = "Hello Bob!";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
    auto encrypted_bytes = alice_bridge->encrypt_message(bob_rdx, message_bytes);

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.id = "test_event_id";
    event_data.pubkey = "alice_pubkey";
    event_data.created_at = test_timestamp;
    event_data.kind = radix_relay::nostr::protocol::kind::encrypted_message;
    event_data.content = hex_content;
    event_data.sig = "signature";
    event_data.tags.push_back({ "p", alice_rdx });

    const radix_relay::nostr::events::incoming::encrypted_message event{ event_data };

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->sender_rdx == alice_rdx);
      CHECK(result->content == plaintext);
      CHECK(result->timestamp == test_timestamp);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("message_handler handles incoming bundle_announcement without establishing session", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_bundle_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_bundle_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.pubkey = "alice_nostr_pubkey";
    event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
    event_data.content = alice_bundle_base64;
    event_data.created_at = test_timestamp;
    event_data.id = "bundle_event_id";
    event_data.sig = "bundle_signature";

    const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };

    const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->pubkey == "alice_nostr_pubkey");
      CHECK(result->bundle_content == alice_bundle_base64);
      CHECK(result->event_id == "bundle_event_id");
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("message_handler handles send command", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_send_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_send_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();

    auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    const std::string plaintext = "Hello Bob!";

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
    auto [event_id, bytes] = handler.handle(radix_relay::core::events::send{ .peer = bob_rdx, .message = plaintext });

    CHECK_FALSE(event_id.empty());

    std::string json_str;
    json_str.reserve(bytes.size());
    std::ranges::transform(
      bytes, std::back_inserter(json_str), [](std::byte byte) { return std::bit_cast<char>(byte); });
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

TEST_CASE("message_handler handles publish_identity command", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_publish_alice.db";
  std::filesystem::remove(alice_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
    auto [event_id, bytes] = handler.handle(radix_relay::core::events::publish_identity{});

    CHECK_FALSE(event_id.empty());

    std::string json_str;
    json_str.reserve(bytes.size());
    std::ranges::transform(
      bytes, std::back_inserter(json_str), [](std::byte byte) { return std::bit_cast<char>(byte); });
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

TEST_CASE("message_handler handles establish_session command", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_establish_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_establish_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(radix_relay::core::events::establish_session{ .bundle_data = alice_bundle_base64 });

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->peer_rdx.starts_with("RDX:"));
      CHECK(result->peer_rdx.length() == 68);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("message_handler handles trust command", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_trust_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_trust_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].template get<std::string>();

    auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "");

    const std::string alias = "bob_alias";

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
    handler.handle(radix_relay::core::events::trust{ .peer = bob_rdx, .alias = alias });

    auto contact = alice_bridge->lookup_contact(alias);
    CHECK(contact.rdx_fingerprint == bob_rdx);
    CHECK(contact.user_alias == alias);
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}
