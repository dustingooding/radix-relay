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

    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();
    auto alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();
    auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    const std::string plaintext = "Hello Bob!";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
    auto encrypted_bytes = alice_bridge->encrypt_message(bob_rdx, message_bytes);

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    auto alice_contact = bob_bridge->lookup_contact(alice_rdx);
    const std::string alice_nostr_pubkey = alice_contact.nostr_pubkey;

    auto bob_contact = alice_bridge->lookup_contact(bob_rdx);
    const std::string bob_nostr_pubkey = bob_contact.nostr_pubkey;

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.id = "test_event_id";
    event_data.pubkey = alice_nostr_pubkey;
    event_data.created_at = test_timestamp;
    event_data.kind = radix_relay::nostr::protocol::kind::encrypted_message;
    event_data.content = hex_content;
    event_data.sig = "signature";
    event_data.tags.push_back({ "p", bob_nostr_pubkey });

    const radix_relay::nostr::events::incoming::encrypted_message event{ event_data };

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->sender_rdx == alice_rdx);
      CHECK(result->sender_alias == "alice");
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

    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.pubkey = "alice_nostr_pubkey";
    event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
    event_data.content = alice_bundle_base64;
    event_data.created_at = test_timestamp;
    event_data.id = "bundle_event_id";
    event_data.sig = "bundle_signature";
    event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });
    event_data.tags.push_back({ "radix_version", "0.4.0" });

    const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };

    const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      REQUIRE(std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(*result));
      const auto &announcement = std::get<radix_relay::core::events::bundle_announcement_received>(*result);
      CHECK(announcement.pubkey == "alice_nostr_pubkey");
      CHECK(announcement.bundle_content == alice_bundle_base64);
      CHECK(announcement.event_id == "bundle_event_id");
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("message_handler filters old version bundle announcements", "[message_handler]")
{
  const std::string alice_path = "/tmp/nostr_handler_version_filter_alice.db";
  constexpr std::uint64_t test_timestamp = 1234567890;

  std::filesystem::remove(alice_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);

    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("0.4.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    SECTION("rejects bundle announcements with version < 0.4.0")
    {
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "old_client_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = alice_bundle_base64;
      event_data.created_at = test_timestamp;
      event_data.id = "old_bundle_event_id";
      event_data.sig = "old_bundle_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });
      event_data.tags.push_back({ "radix_version", "0.3.9" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
      auto result = handler.handle(event);

      REQUIRE_FALSE(result.has_value());
    }

    SECTION("accepts bundle announcements with version >= 0.4.0")
    {
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "new_client_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = alice_bundle_base64;
      event_data.created_at = test_timestamp;
      event_data.id = "new_bundle_event_id";
      event_data.sig = "new_bundle_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });
      event_data.tags.push_back({ "radix_version", "0.4.0" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
      auto result = handler.handle(event);

      REQUIRE(result.has_value());
      if (result.has_value()) {
        REQUIRE(std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(*result));
        const auto &announcement = std::get<radix_relay::core::events::bundle_announcement_received>(*result);
        CHECK(announcement.pubkey == "new_client_pubkey");
        CHECK(announcement.bundle_content == alice_bundle_base64);
        CHECK(announcement.event_id == "new_bundle_event_id");
      }
    }

    SECTION("rejects bundle announcements with missing version tag")
    {
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "no_version_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = alice_bundle_base64;
      event_data.created_at = test_timestamp;
      event_data.id = "no_version_event_id";
      event_data.sig = "no_version_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
      auto result = handler.handle(event);

      REQUIRE_FALSE(result.has_value());
    }
  }

  std::filesystem::remove(alice_path);
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

    auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
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
    const bool found_version_tag =
      std::ranges::any_of(tags, [](const auto &tag) { return tag.size() >= 2 and tag[0] == "radix_version"; });
    CHECK(found_p_tag);
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
    auto result = handler.handle(radix_relay::core::events::publish_identity{});

    CHECK_FALSE(result.event_id.empty());

    std::string json_str;
    json_str.reserve(result.bytes.size());
    std::ranges::transform(
      result.bytes, std::back_inserter(json_str), [](std::byte byte) { return std::bit_cast<char>(byte); });
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

    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
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

    auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
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

TEST_CASE("message_handler filters bundle announcements by version", "[message_handler][version]")
{
  const std::string test_path = "/tmp/nostr_handler_version_filter.db";
  std::filesystem::remove(test_path);

  {
    auto bridge = std::make_shared<radix_relay::signal::bridge>(test_path);
    const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bridge);

    SECTION("ignores bundles with version < 0.4.0")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "test_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = "test_bundle_content";
      event_data.created_at = test_timestamp;
      event_data.id = "test_event_id";
      event_data.sig = "test_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });
      event_data.tags.push_back({ "radix_version", "0.3.0" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE_FALSE(result.has_value());
    }

    SECTION("accepts bundles with version == 0.4.0")
    {
      auto bundle_json_info = bridge->generate_prekey_bundle_announcement("0.4.0");
      auto event_json = nlohmann::json::parse(bundle_json_info.announcement_json);

      radix_relay::nostr::protocol::event_data event_data;
      event_data.id = event_json["id"];
      event_data.pubkey = event_json["pubkey"];
      event_data.created_at = event_json["created_at"];
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = event_json["content"];
      event_data.sig = event_json["sig"];

      for (const auto &tag : event_json["tags"]) {
        std::vector<std::string> tag_vec;
        std::ranges::transform(
          tag, std::back_inserter(tag_vec), [](const auto &item) { return item.template get<std::string>(); });
        event_data.tags.push_back(tag_vec);
      }

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE(result.has_value());
    }

    SECTION("accepts bundles with version > 0.4.0")
    {
      auto bundle_json_info = bridge->generate_prekey_bundle_announcement("0.5.0");
      auto event_json = nlohmann::json::parse(bundle_json_info.announcement_json);

      radix_relay::nostr::protocol::event_data event_data;
      event_data.id = event_json["id"];
      event_data.pubkey = event_json["pubkey"];
      event_data.created_at = event_json["created_at"];
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = event_json["content"];
      event_data.sig = event_json["sig"];

      for (const auto &tag : event_json["tags"]) {
        std::vector<std::string> tag_vec;
        std::ranges::transform(
          tag, std::back_inserter(tag_vec), [](const auto &item) { return item.template get<std::string>(); });
        event_data.tags.push_back(tag_vec);
      }

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE(result.has_value());
    }

    SECTION("ignores bundles without version tag")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "test_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = "test_bundle_content";
      event_data.created_at = test_timestamp;
      event_data.id = "test_event_id";
      event_data.sig = "test_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE_FALSE(result.has_value());
    }
  }

  std::filesystem::remove(test_path);
}

TEST_CASE("message_handler filters bundle announcements by content", "[message_handler][content]")
{
  const std::string test_path = "/tmp/nostr_handler_content_filter.db";
  std::filesystem::remove(test_path);

  {
    auto bridge = std::make_shared<radix_relay::signal::bridge>(test_path);
    const radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bridge);

    SECTION("returns bundle_announcement_removed for empty content")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      radix_relay::nostr::protocol::event_data event_data;
      event_data.pubkey = "test_pubkey";
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = "";
      event_data.created_at = test_timestamp;
      event_data.id = "test_event_id";
      event_data.sig = "test_signature";
      event_data.tags.push_back({ "d", "radix_prekey_bundle_v1" });
      event_data.tags.push_back({ "radix_version", "0.4.0" });

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE(result.has_value());
      if (result.has_value()) {
        REQUIRE(std::holds_alternative<radix_relay::core::events::bundle_announcement_removed>(*result));
        const auto &removed = std::get<radix_relay::core::events::bundle_announcement_removed>(*result);
        CHECK(removed.pubkey == "test_pubkey");
        CHECK(removed.event_id == "test_event_id");
      }
    }

    SECTION("accepts bundles with non-empty content")
    {
      auto bundle_json_info = bridge->generate_prekey_bundle_announcement("0.4.0");
      auto event_json = nlohmann::json::parse(bundle_json_info.announcement_json);

      radix_relay::nostr::protocol::event_data event_data;
      event_data.id = event_json["id"];
      event_data.pubkey = event_json["pubkey"];
      event_data.created_at = event_json["created_at"];
      event_data.kind = radix_relay::nostr::protocol::kind::bundle_announcement;
      event_data.content = event_json["content"];
      event_data.sig = event_json["sig"];

      for (const auto &tag : event_json["tags"]) {
        std::vector<std::string> tag_vec;
        std::ranges::transform(
          tag, std::back_inserter(tag_vec), [](const auto &item) { return item.template get<std::string>(); });
        event_data.tags.push_back(tag_vec);
      }

      const radix_relay::nostr::events::incoming::bundle_announcement event{ event_data };
      auto result = handler.handle(event);

      REQUIRE(result.has_value());
      if (result.has_value()) {
        REQUIRE(std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(*result));
        const auto &announcement = std::get<radix_relay::core::events::bundle_announcement_received>(*result);
        CHECK_FALSE(announcement.bundle_content.empty());
      }
    }
  }

  std::filesystem::remove(test_path);
}

TEST_CASE("message_handler returns should_republish_bundle flag in message_received", "[message_handler][republishing]")
{
  const std::string alice_path = "/tmp/nostr_handler_republish_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_republish_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();
    auto alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].get<std::string>();
    auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    const std::string plaintext = "Hello Bob! This should consume your pre-key.";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
    auto encrypted_bytes = alice_bridge->encrypt_message(bob_rdx, message_bytes);

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    auto alice_contact = bob_bridge->lookup_contact(alice_rdx);
    const std::string alice_nostr_pubkey = alice_contact.nostr_pubkey;

    auto bob_contact = alice_bridge->lookup_contact(bob_rdx);
    const std::string bob_nostr_pubkey = bob_contact.nostr_pubkey;

    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.id = "test_event_id";
    event_data.pubkey = alice_nostr_pubkey;
    event_data.created_at = test_timestamp;
    event_data.kind = radix_relay::nostr::protocol::kind::encrypted_message;
    event_data.content = hex_content;
    event_data.sig = "signature";
    event_data.tags.push_back({ "p", bob_nostr_pubkey });

    const radix_relay::nostr::events::incoming::encrypted_message event{ event_data };

    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(bob_bridge);
    auto result = handler.handle(event);

    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->sender_rdx == alice_rdx);
      CHECK(result->sender_alias == "alice");
      CHECK(result->content == plaintext);
      CHECK(result->timestamp == test_timestamp);
      CHECK(result->should_republish_bundle);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}

TEST_CASE("message_handler handles encrypted_message from unknown sender", "[message_handler][x3dh][unknown-sender]")
{
  const std::string alice_path = "/tmp/nostr_handler_unknown_alice.db";
  const std::string bob_path = "/tmp/nostr_handler_unknown_bob.db";

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    // Alice publishes her bundle
    auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto alice_event_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_event_json["content"].get<std::string>();

    // Bob gets Alice's bundle and establishes session
    auto alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "Alice");

    // Bob gets Alice's Nostr pubkey for the event
    auto alice_contact = bob_bridge->lookup_contact(alice_rdx);
    const std::string alice_nostr_pubkey = alice_contact.nostr_pubkey;

    // Bob encrypts a message to Alice
    const std::string plaintext = "Hello Alice! I'm Bob from the future.";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
    auto encrypted_bytes = bob_bridge->encrypt_message(alice_rdx, message_bytes);

    // Convert to hex for Nostr event
    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    // Get Bob's Nostr pubkey (this will be in the event)
    auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_info.announcement_json);

    // Extract Bob's pubkey from his event
    const std::string bob_nostr_pubkey = bob_event_json["pubkey"].get<std::string>();

    // CRITICAL: Alice has NEVER seen Bob before - no contact, no session
    auto alice_contacts_before = alice_bridge->list_contacts();
    auto contacts_before_count = alice_contacts_before.size();

    // Create the Nostr event as if it came from Bob
    constexpr std::uint64_t test_timestamp = 1234567890;
    radix_relay::nostr::protocol::event_data event_data;
    event_data.id = "test_event_id";
    event_data.pubkey = bob_nostr_pubkey;// Bob's Nostr pubkey
    event_data.created_at = test_timestamp;
    event_data.kind = radix_relay::nostr::protocol::kind::encrypted_message;
    event_data.content = hex_content;
    event_data.sig = "signature";
    event_data.tags.push_back({ "p", alice_nostr_pubkey });

    const radix_relay::nostr::events::incoming::encrypted_message event{ event_data };

    // Alice handles the message from unknown sender Bob
    radix_relay::nostr::message_handler<radix_relay::signal::bridge> handler(alice_bridge);
    auto result = handler.handle(event);

    // Assert: Message was successfully decrypted
    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->content == plaintext);
      CHECK(result->timestamp == test_timestamp);
      CHECK(result->should_republish_bundle);// Pre-key was consumed

      // Assert: Bob is now in Alice's contacts
      auto alice_contacts_after = alice_bridge->list_contacts();
      CHECK(alice_contacts_after.size() == contacts_before_count + 1);

      // Assert: Contact info is correct
      CHECK(result->sender_alias.starts_with("Unknown-"));

      // Verify Bob's contact was created correctly
      auto bob_contact = alice_bridge->lookup_contact(bob_nostr_pubkey);
      CHECK(bob_contact.nostr_pubkey == bob_nostr_pubkey);
      CHECK(bob_contact.user_alias.starts_with("Unknown-"));
      CHECK(bob_contact.has_active_session);

      // Assert: Alice can now send a reply to Bob
      const std::string response = "Hi Bob! Nice to hear from you.";
      const std::vector<uint8_t> response_bytes(response.begin(), response.end());
      auto response_encrypted = alice_bridge->encrypt_message(bob_contact.rdx_fingerprint, response_bytes);
      CHECK(!response_encrypted.empty());

      // Bob can decrypt Alice's response
      auto bob_result = bob_bridge->decrypt_message_with_metadata(alice_nostr_pubkey, response_encrypted);
      std::string bob_decrypted(bob_result.plaintext.begin(), bob_result.plaintext.end());
      CHECK(bob_decrypted == response);
    }
  }

  std::filesystem::remove(alice_path);
  std::filesystem::remove(bob_path);
}
