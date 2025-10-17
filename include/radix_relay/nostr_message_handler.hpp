#pragma once

#include <algorithm>
#include <bit>
#include <ctime>
#include <fmt/format.h>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <radix_relay/events/events.hpp>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <signal_bridge_cxx/lib.h>
#include <string>
#include <vector>

namespace radix_relay {

class nostr_message_handler
{
public:
  explicit nostr_message_handler(SignalBridge *bridge) : bridge_(bridge) {}

  // Incoming Nostr events (called by Session on session_strand)
  // Returns optional event to post to main_strand
  auto handle(const nostr::events::incoming::encrypted_message &event) -> std::optional<events::message_received>
  {
    auto p_tag = std::ranges::find_if(event.tags, [](const auto &tag) { return tag.size() >= 2 && tag[0] == "p"; });

    if (p_tag == event.tags.end()) { return std::nullopt; }

    const std::string sender_rdx = (*p_tag)[1];

    std::vector<uint8_t> encrypted_bytes;
    constexpr int hex_base = 16;
    for (size_t i = 0; i < event.content.length(); i += 2) {
      auto byte_string = event.content.substr(i, 2);
      auto byte_value = static_cast<uint8_t>(std::stoul(byte_string, nullptr, hex_base));
      encrypted_bytes.push_back(byte_value);
    }

    auto decrypted_bytes = radix_relay::decrypt_message(
      *bridge_, sender_rdx.c_str(), rust::Slice<const uint8_t>{ encrypted_bytes.data(), encrypted_bytes.size() });

    const std::string decrypted_content(decrypted_bytes.begin(), decrypted_bytes.end());

    return events::message_received{
      .sender_rdx = sender_rdx, .content = decrypted_content, .timestamp = event.created_at
    };
  }

  auto handle(const nostr::events::incoming::bundle_announcement &event) -> std::optional<events::session_established>
  {
    auto peer_rdx = radix_relay::add_contact_and_establish_session_from_base64(*bridge_, event.content.c_str(), "");
    return events::session_established{ std::string(peer_rdx) };
  }

  // Command events (called by Session on session_strand)
  // Returns pair of <event_id, bytes> to track and send
  auto handle(const events::send &cmd) -> std::pair<std::string, std::vector<std::byte>>
  {
    std::vector<uint8_t> plaintext_bytes(cmd.message.begin(), cmd.message.end());
    auto encrypted_bytes = radix_relay::encrypt_message(
      *bridge_, cmd.peer.c_str(), rust::Slice<const uint8_t>{ plaintext_bytes.data(), plaintext_bytes.size() });

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    auto signed_event_json = radix_relay::create_and_sign_encrypted_message(
      *bridge_, cmd.peer.c_str(), hex_content.c_str(), static_cast<std::uint32_t>(std::time(nullptr)), "0.1.0");

    auto event_json = nlohmann::json::parse(signed_event_json);
    const std::string event_id = event_json["id"].get<std::string>();

    nostr::protocol::event_data event_data;
    event_data.id = event_id;
    event_data.pubkey = event_json["pubkey"].get<std::string>();
    event_data.created_at = event_json["created_at"].get<std::uint64_t>();
    event_data.kind = event_json["kind"].get<nostr::protocol::kind>();
    event_data.content = event_json["content"].get<std::string>();
    event_data.sig = event_json["sig"].get<std::string>();

    for (const auto &tag : event_json["tags"]) {
      std::vector<std::string> tag_vec;
      std::ranges::transform(
        tag, std::back_inserter(tag_vec), [](const auto &item) { return item.template get<std::string>(); });
      event_data.tags.push_back(tag_vec);
    }

    auto protocol_event = nostr::protocol::event::from_event_data(event_data);
    auto json_str = protocol_event.serialize();

    std::vector<std::byte> bytes;
    bytes.resize(json_str.size());
    std::ranges::transform(json_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    return { event_id, bytes };
  }

  auto handle(const events::publish_identity & /*command*/) -> std::pair<std::string, std::vector<std::byte>>
  {
    const std::string version_str = "0.1.0";
    auto bundle_json = radix_relay::generate_prekey_bundle_announcement(*bridge_, version_str.c_str());
    auto event_json = nlohmann::json::parse(bundle_json);

    const std::string event_id = event_json["id"].get<std::string>();

    nostr::protocol::event_data event_data;
    event_data.id = event_id;
    event_data.pubkey = event_json["pubkey"].get<std::string>();
    event_data.created_at = event_json["created_at"].get<std::uint64_t>();
    event_data.kind = event_json["kind"].get<nostr::protocol::kind>();
    event_data.content = event_json["content"].get<std::string>();
    event_data.sig = event_json["sig"].get<std::string>();

    for (const auto &tag : event_json["tags"]) {
      std::vector<std::string> tag_vec;
      std::ranges::transform(
        tag, std::back_inserter(tag_vec), [](const auto &item) { return item.template get<std::string>(); });
      event_data.tags.push_back(tag_vec);
    }

    auto protocol_event = nostr::protocol::event::from_event_data(event_data);
    auto json_str = protocol_event.serialize();

    std::vector<std::byte> bytes;
    bytes.resize(json_str.size());
    std::ranges::transform(json_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    return { event_id, bytes };
  }

  // Local operation (no networking, just updates DB)
  auto handle(const events::trust &cmd) -> void
  {
    radix_relay::assign_contact_alias(*bridge_, cmd.peer.c_str(), cmd.alias.c_str());
  }

  // Subscription request returns subscription_id + bytes
  static auto handle(const events::subscribe &cmd) -> std::pair<std::string, std::vector<std::byte>>
  {
    std::vector<std::byte> bytes;
    bytes.resize(cmd.subscription_json.size());
    std::ranges::transform(
      cmd.subscription_json, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    auto parsed = nlohmann::json::parse(cmd.subscription_json);
    const std::string subscription_id = parsed[1].get<std::string>();

    return { subscription_id, bytes };
  }

  // Stubs for events we don't handle yet (required by NostrHandler concept)
  static auto handle(const nostr::events::incoming::ok & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::eose & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::unknown_message & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::unknown_protocol & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::identity_announcement & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::session_request & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::node_status & /*event*/) -> void {}

private:
  SignalBridge *bridge_;
};

}// namespace radix_relay
