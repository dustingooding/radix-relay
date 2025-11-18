#pragma once

#include <core/contact_info.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace radix_relay::concepts {

template<typename T>
concept signal_bridge = requires(T bridge,
  const std::string &rdx,
  const std::string &alias,
  const std::string &bundle,
  const std::string &version,
  const std::string &content,
  const std::vector<uint8_t> &bytes,
  const std::string &subscription_id,
  uint32_t timestamp,
  std::uint64_t since_timestamp) {
  // Identity and session management
  { bridge.get_node_fingerprint() } -> std::convertible_to<std::string>;
  { bridge.list_contacts() } -> std::convertible_to<std::vector<radix_relay::core::contact_info>>;

  // Message encryption/decryption
  { bridge.encrypt_message(rdx, bytes) } -> std::convertible_to<std::vector<uint8_t>>;
  { bridge.decrypt_message(rdx, bytes) } -> std::convertible_to<std::vector<uint8_t>>;

  // Session establishment
  { bridge.add_contact_and_establish_session_from_base64(bundle, alias) } -> std::convertible_to<std::string>;

  // Bundle generation
  { bridge.generate_prekey_bundle_announcement(version) } -> std::convertible_to<std::string>;
  { bridge.extract_rdx_from_bundle_base64(bundle) } -> std::convertible_to<std::string>;

  // Contact management
  { bridge.assign_contact_alias(rdx, alias) } -> std::same_as<void>;
  { bridge.lookup_contact(alias) } -> std::convertible_to<radix_relay::core::contact_info>;

  // Nostr signing
  { bridge.create_and_sign_encrypted_message(rdx, content, timestamp, version) } -> std::convertible_to<std::string>;
  { bridge.sign_nostr_event(content) } -> std::convertible_to<std::string>;

  // Nostr subscription
  { bridge.create_subscription_for_self(subscription_id, since_timestamp) } -> std::convertible_to<std::string>;
  { bridge.update_last_message_timestamp(since_timestamp) } -> std::same_as<void>;
};

}// namespace radix_relay::concepts
