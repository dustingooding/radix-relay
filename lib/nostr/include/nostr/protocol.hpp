#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace radix_relay::nostr::protocol {

inline constexpr auto bundle_announcement_minimum_version = "0.4.0";

enum class kind : std::uint16_t {
  profile_metadata = 0,
  text_note = 1,
  recommend_relay = 2,
  contact_list = 3,
  encrypted_dm = 4,
  reaction = 7,

  parameterized_replaceable_start = 30000,
  bundle_announcement = 30078,

  encrypted_message = 40001,
  identity_announcement = 40002,
  session_request = 40003,
  node_status = 40004,
};

struct event_data
{
  std::string id;
  std::string pubkey;
  std::uint64_t created_at{};
  enum kind kind {};
  std::vector<std::vector<std::string>> tags;
  std::string content;
  std::string sig;

  static auto deserialize(std::span<const std::byte> bytes) -> std::optional<event_data>;

  static auto deserialize(const std::string &json) -> std::optional<event_data>;

  [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

  [[nodiscard]] static auto create_identity_announcement(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &signal_fingerprint,
    const std::string &capabilities = "mesh,nostr") -> event_data;

  [[nodiscard]] static auto create_bundle_announcement(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &bundle_hex) -> event_data;

  [[nodiscard]] static auto create_encrypted_message(std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &encrypted_payload,
    const std::string &session_id) -> event_data;

  [[nodiscard]] static auto create_session_request(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &prekey_bundle) -> event_data;

  [[nodiscard]] auto is_radix_message() const -> bool;

  [[nodiscard]] auto get_kind() const -> std::optional<enum kind>;
};

struct ok
{
  std::string event_id;
  bool accepted{};
  std::string message;

  static auto deserialize(const std::string &json) -> std::optional<ok>;
};

struct eose
{
  std::string subscription_id;

  static auto deserialize(const std::string &json) -> std::optional<eose>;
};

struct req
{
  std::string subscription_id;
  nlohmann::json filters;

  [[nodiscard]] auto serialize() const -> std::string;

  static auto deserialize(const std::string &json) -> std::optional<req>;
};

struct event
{
  std::string subscription_id;
  event_data data;

  static auto from_event_data(const event_data &evt) -> event;

  [[nodiscard]] auto serialize() const -> std::string;

  static auto deserialize(const std::string &json) -> std::optional<event>;
};

constexpr std::size_t max_subscription_id_length = 64;

inline auto validate_subscription_id(const std::string &subscription_id) -> void
{
  if (subscription_id.empty()) { throw std::invalid_argument("Subscription ID cannot be empty"); }
  if (subscription_id.length() > max_subscription_id_length) {
    throw std::invalid_argument("Subscription ID exceeds maximum length of 64 characters");
  }
}

}// namespace radix_relay::nostr::protocol
