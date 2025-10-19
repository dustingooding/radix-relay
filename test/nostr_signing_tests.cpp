#include "signal_bridge_cxx/lib.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <radix_relay/nostr_protocol.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <ranges>

// Helper function to convert string literals to byte spans for testing
auto string_to_bytes(const std::string &str) -> std::vector<std::byte>
{
  std::vector<std::byte> bytes;
  bytes.resize(str.size());
  std::ranges::transform(str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
  return bytes;
}

TEST_CASE("Nostr Event Signing via Signal Bridge", "[nostr][signing]")
{
  constexpr auto test_timestamp = 1234567890U;

  SECTION("sign_nostr_event produces valid signed event")
  {
    const auto db_path =
      (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_nostr_signing.db").string();

    {
      auto signal_bridge = radix_relay::new_signal_bridge(db_path.c_str());

      auto event = radix_relay::nostr::protocol::event_data::create_encrypted_message(
        test_timestamp, "test_recipient", "encrypted_test_content", "test_session");

      auto json_bytes = event.serialize();
      std::string event_json;
      event_json.resize(json_bytes.size());
      std::ranges::transform(json_bytes, event_json.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

      auto signed_event_json = radix_relay::sign_nostr_event(*signal_bridge, event_json);

      auto signed_json_bytes = string_to_bytes(std::string(signed_event_json));
      auto signed_event = radix_relay::nostr::protocol::event_data::deserialize(signed_json_bytes);

      REQUIRE(signed_event.has_value());
      if (signed_event.has_value()) {
        const auto &evt = signed_event.value();

        CHECK_FALSE(evt.id.empty());
        CHECK_FALSE(evt.pubkey.empty());
        CHECK_FALSE(evt.sig.empty());

        CHECK(evt.id.length() == 64);// SHA256 hex
        CHECK(evt.pubkey.length() == 64);// Nostr pubkey hex

        CHECK(evt.content == "encrypted_test_content");
        CHECK(evt.created_at == 1234567890);
        CHECK(evt.kind == radix_relay::nostr::protocol::kind::encrypted_message);
      }
    }

    std::ignore = std::remove(db_path.c_str());
  }

  SECTION("signing is deterministic for identical events")
  {
    const auto db_path =
      (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_nostr_deterministic.db").string();

    {
      auto signal_bridge = radix_relay::new_signal_bridge(db_path.c_str());

      auto event1 = radix_relay::nostr::protocol::event_data::create_encrypted_message(
        test_timestamp, "test_recipient", "test_content", "test_session");
      auto event2 = radix_relay::nostr::protocol::event_data::create_encrypted_message(
        test_timestamp, "test_recipient", "test_content", "test_session");

      auto json1_bytes = event1.serialize();
      auto json2_bytes = event2.serialize();
      std::string json1;
      std::string json2;
      json1.resize(json1_bytes.size());
      json2.resize(json2_bytes.size());
      std::ranges::transform(json1_bytes, json1.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });
      std::ranges::transform(json2_bytes, json2.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

      auto signed1 = radix_relay::sign_nostr_event(*signal_bridge, json1);
      auto signed2 = radix_relay::sign_nostr_event(*signal_bridge, json2);

      auto event1_signed = radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(std::string(signed1)));
      auto event2_signed = radix_relay::nostr::protocol::event_data::deserialize(string_to_bytes(std::string(signed2)));

      REQUIRE(event1_signed.has_value());
      REQUIRE(event2_signed.has_value());
      if (event1_signed.has_value() and event2_signed.has_value()) {
        const auto &evt1 = event1_signed.value();
        const auto &evt2 = event2_signed.value();

        CHECK(evt1.id == evt2.id);
        CHECK(evt1.pubkey == evt2.pubkey);
        CHECK_FALSE(evt1.sig.empty());
        CHECK_FALSE(evt2.sig.empty());
      }
    }

    std::ignore = std::remove(db_path.c_str());
  }
}
