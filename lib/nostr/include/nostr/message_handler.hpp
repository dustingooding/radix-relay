#pragma once

#include <algorithm>
#include <bit>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <core/semver_utils.hpp>
#include <ctime>
#include <fmt/format.h>
#include <internal_use_only/config.hpp>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <nostr/events.hpp>
#include <nostr/protocol.hpp>
#include <nostr/semver_utils.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace radix_relay::nostr {

struct publish_bundle_result
{
  std::string event_id;
  std::vector<std::byte> bytes;
  std::uint32_t pre_key_id;
  std::uint32_t signed_pre_key_id;
  std::uint32_t kyber_pre_key_id;
};

template<concepts::signal_bridge Bridge> class message_handler
{
public:
  explicit message_handler(std::shared_ptr<Bridge> bridge) : bridge_(bridge) {}

  // Incoming Nostr events (called by Session on session_strand)
  // Returns optional event to post to main_strand
  [[nodiscard]] auto handle(const nostr::events::incoming::encrypted_message &event)
    -> std::optional<core::events::message_received>
  {
    std::vector<uint8_t> encrypted_bytes;
    constexpr int hex_base = 16;
    for (size_t i = 0; i < event.content.length(); i += 2) {
      auto byte_string = event.content.substr(i, 2);
      auto byte_value = static_cast<uint8_t>(std::stoul(byte_string, nullptr, hex_base));
      encrypted_bytes.push_back(byte_value);
    }

    // Pass Nostr pubkey as peer_hint - decrypt_message_with_metadata will:
    // - For PreKeySignalMessage: extract identity key and create contact automatically
    // - For SignalMessage: look up existing contact by this pubkey
    auto result = bridge_->decrypt_message_with_metadata(event.pubkey, encrypted_bytes);

    const std::string decrypted_content(result.plaintext.begin(), result.plaintext.end());

    bridge_->update_last_message_timestamp(event.created_at);

    // After successful decryption, get the sender's contact info (now guaranteed to exist)
    auto sender_contact = bridge_->lookup_contact(event.pubkey);

    return core::events::message_received{ .sender_rdx = sender_contact.rdx_fingerprint,
      .sender_alias = sender_contact.user_alias,
      .content = decrypted_content,
      .timestamp = event.created_at,
      .should_republish_bundle = result.should_republish_bundle };
  }

  [[nodiscard]] static auto handle(const nostr::events::incoming::bundle_announcement &event) -> std::optional<
    std::variant<core::events::bundle_announcement_received, core::events::bundle_announcement_removed>>
  {
    auto version = nostr::extract_version_from_tags(event.tags);
    if (not version.has_value()) { return std::nullopt; }

    if (not core::is_version_compatible(version.value(), protocol::bundle_announcement_minimum_version)) {
      return std::nullopt;
    }

    if (event.content.empty()) {
      return core::events::bundle_announcement_removed{ .pubkey = event.pubkey, .event_id = event.id };
    }

    return core::events::bundle_announcement_received{
      .pubkey = event.pubkey, .bundle_content = event.content, .event_id = event.id
    };
  }

  // Command events (called by Session on session_strand)
  // Returns pair of <event_id, bytes> to track and send
  [[nodiscard]] auto handle(const core::events::send &cmd) -> std::pair<std::string, std::vector<std::byte>>
  {
    std::vector<uint8_t> plaintext_bytes(cmd.message.begin(), cmd.message.end());
    auto encrypted_bytes = bridge_->encrypt_message(cmd.peer, plaintext_bytes);

    std::string hex_content;
    for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

    auto signed_event_json = bridge_->create_and_sign_encrypted_message(
      cmd.peer, hex_content, static_cast<std::uint32_t>(std::time(nullptr)), std::string{ cmake::project_version });

    auto event_json = nlohmann::json::parse(signed_event_json);
    const std::string event_id = event_json["id"].template get<std::string>();

    nostr::protocol::event_data event_data;
    event_data.id = event_id;
    event_data.pubkey = event_json["pubkey"].template get<std::string>();
    event_data.created_at = event_json["created_at"].template get<std::uint64_t>();
    event_data.kind = event_json["kind"].template get<nostr::protocol::kind>();
    event_data.content = event_json["content"].template get<std::string>();
    event_data.sig = event_json["sig"].template get<std::string>();

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

  [[nodiscard]] auto handle(const core::events::publish_identity & /*command*/) -> publish_bundle_result
  {
    const std::string version_str{ radix_relay::cmake::project_version };
    auto bundle_info = bridge_->generate_prekey_bundle_announcement(version_str);
    auto event_json = nlohmann::json::parse(bundle_info.announcement_json);

    const std::string event_id = event_json["id"].template get<std::string>();

    nostr::protocol::event_data event_data;
    event_data.id = event_id;
    event_data.pubkey = event_json["pubkey"].template get<std::string>();
    event_data.created_at = event_json["created_at"].template get<std::uint64_t>();
    event_data.kind = event_json["kind"].template get<nostr::protocol::kind>();
    event_data.content = event_json["content"].template get<std::string>();
    event_data.sig = event_json["sig"].template get<std::string>();

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

    return publish_bundle_result{ .event_id = event_id,
      .bytes = std::move(bytes),
      .pre_key_id = bundle_info.pre_key_id,
      .signed_pre_key_id = bundle_info.signed_pre_key_id,
      .kyber_pre_key_id = bundle_info.kyber_pre_key_id };
  }

  [[nodiscard]] auto handle(const core::events::unpublish_identity & /*command*/)
    -> std::pair<std::string, std::vector<std::byte>>
  {
    const std::string version_str{ radix_relay::cmake::project_version };
    auto bundle_json = bridge_->generate_empty_bundle_announcement(version_str);
    auto event_json = nlohmann::json::parse(bundle_json);

    const std::string event_id = event_json["id"].template get<std::string>();

    nostr::protocol::event_data event_data;
    event_data.id = event_id;
    event_data.pubkey = event_json["pubkey"].template get<std::string>();
    event_data.created_at = event_json["created_at"].template get<std::uint64_t>();
    event_data.kind = event_json["kind"].template get<nostr::protocol::kind>();
    event_data.content = event_json["content"].template get<std::string>();
    event_data.sig = event_json["sig"].template get<std::string>();

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
  auto handle(const core::events::trust &cmd) -> void { bridge_->assign_contact_alias(cmd.peer, cmd.alias); }

  // Establish session from bundle data (returns session_established event)
  [[nodiscard]] auto handle(const core::events::establish_session &cmd)
    -> std::optional<core::events::session_established>
  {
    auto peer_rdx = bridge_->add_contact_and_establish_session_from_base64(cmd.bundle_data, "");
    return core::events::session_established{ peer_rdx };
  }

  // Subscription request returns subscription_id + bytes
  [[nodiscard]] static auto handle(const core::events::subscribe &cmd) -> std::pair<std::string, std::vector<std::byte>>
  {
    std::vector<std::byte> bytes;
    bytes.resize(cmd.subscription_json.size());
    std::ranges::transform(
      cmd.subscription_json, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });

    auto parsed = nlohmann::json::parse(cmd.subscription_json);
    const std::string subscription_id = parsed[1].get<std::string>();

    return { subscription_id, bytes };
  }

  static auto handle(const nostr::events::incoming::ok &event) -> void
  {
    spdlog::debug("[nostr_handler] OK received: event_id={}, accepted={}, message={}",
      event.event_id,
      event.accepted,
      event.message);
  }

  static auto handle(const nostr::events::incoming::eose &event) -> void
  {
    spdlog::debug("[nostr_handler] EOSE received: subscription_id={}", event.subscription_id);
  }

  static auto handle(const nostr::events::incoming::unknown_message &event) -> void
  {
    spdlog::warn("[nostr_handler] Unknown message kind: {}", static_cast<std::uint16_t>(event.kind));
  }

  static auto handle(const nostr::events::incoming::unknown_protocol &event) -> void
  {
    spdlog::warn("[nostr_handler] Unknown protocol message: {}", event.message);
  }

  static auto handle(const nostr::events::incoming::identity_announcement & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::session_request & /*event*/) -> void {}
  static auto handle(const nostr::events::incoming::node_status & /*event*/) -> void {}

private:
  std::shared_ptr<Bridge> bridge_;
};

}// namespace radix_relay::nostr
