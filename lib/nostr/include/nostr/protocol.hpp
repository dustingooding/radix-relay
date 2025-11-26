#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace radix_relay::nostr::protocol {

/// Minimum protocol version required for bundle announcements
inline constexpr auto bundle_announcement_minimum_version = "0.4.0";

/**
 * @brief Nostr event kind identifiers.
 *
 * Defines standard Nostr event kinds and Radix Relay custom kinds.
 */
enum class kind : std::uint16_t {
  profile_metadata = 0,///< User profile metadata (NIP-01)
  text_note = 1,///< Text note/post (NIP-01)
  recommend_relay = 2,///< Relay recommendation (NIP-01)
  contact_list = 3,///< Contact list (NIP-02)
  encrypted_dm = 4,///< Encrypted direct message (NIP-04)
  reaction = 7,///< Reaction to an event (NIP-25)

  parameterized_replaceable_start = 30000,///< Start of parameterized replaceable range
  bundle_announcement = 30078,///< Radix: Signal Protocol prekey bundle

  encrypted_message = 40001,///< Radix: Encrypted message via Signal Protocol
  identity_announcement = 40002,///< Radix: Node identity announcement
  session_request = 40003,///< Radix: Session establishment request
  node_status = 40004,///< Radix: Node status update
};

/**
 * @brief Nostr event data structure.
 *
 * Represents a complete Nostr event with all required fields per NIP-01.
 */
struct event_data
{
  std::string id;///< Event ID (32-byte hex hash)
  std::string pubkey;///< Public key of event creator (32-byte hex)
  std::uint64_t created_at{};///< Unix timestamp
  enum kind kind {};///< Event kind identifier
  std::vector<std::vector<std::string>> tags;///< Event tags (arbitrary string arrays)
  std::string content;///< Event content
  std::string sig;///< Schnorr signature (64-byte hex)

  /**
   * @brief Deserializes event data from byte array.
   *
   * @param bytes Raw bytes containing JSON event data
   * @return Parsed event_data or std::nullopt on failure
   */
  static auto deserialize(std::span<const std::byte> bytes) -> std::optional<event_data>;

  /**
   * @brief Deserializes event data from JSON string.
   *
   * @param json JSON string
   * @return Parsed event_data or std::nullopt on failure
   */
  static auto deserialize(const std::string &json) -> std::optional<event_data>;

  /**
   * @brief Serializes event data to byte array.
   *
   * @return Serialized event as byte vector
   */
  [[nodiscard]] auto serialize() const -> std::vector<std::byte>;

  /**
   * @brief Creates an identity announcement event.
   *
   * @param sender_pubkey Nostr public key of sender
   * @param timestamp Unix timestamp
   * @param signal_fingerprint RDX fingerprint
   * @param capabilities Comma-separated capability list
   * @return Constructed event_data
   */
  [[nodiscard]] static auto create_identity_announcement(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &signal_fingerprint,
    const std::string &capabilities = "mesh,nostr") -> event_data;

  /**
   * @brief Creates a bundle announcement event.
   *
   * @param sender_pubkey Nostr public key of sender
   * @param timestamp Unix timestamp
   * @param bundle_hex Hex-encoded prekey bundle
   * @return Constructed event_data
   */
  [[nodiscard]] static auto create_bundle_announcement(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &bundle_hex) -> event_data;

  /**
   * @brief Creates an encrypted message event.
   *
   * @param timestamp Unix timestamp
   * @param recipient_pubkey Nostr public key of recipient
   * @param encrypted_payload Encrypted message content
   * @return Constructed event_data
   */
  [[nodiscard]] static auto create_encrypted_message(std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &encrypted_payload) -> event_data;

  /**
   * @brief Creates a session establishment request event.
   *
   * @param sender_pubkey Nostr public key of sender
   * @param timestamp Unix timestamp
   * @param recipient_pubkey Nostr public key of recipient
   * @param prekey_bundle Prekey bundle data
   * @return Constructed event_data
   */
  [[nodiscard]] static auto create_session_request(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &prekey_bundle) -> event_data;

  /**
   * @brief Checks if event is a Radix-specific message type.
   *
   * @return true if kind is 40001-40004, false otherwise
   */
  [[nodiscard]] auto is_radix_message() const -> bool;

  /**
   * @brief Returns the event kind.
   *
   * @return Event kind or std::nullopt if invalid
   */
  [[nodiscard]] auto get_kind() const -> std::optional<enum kind>;
};

/**
 * @brief Nostr OK response message.
 *
 * Sent by relays to indicate acceptance/rejection of a submitted event.
 */
struct ok
{
  std::string event_id;///< ID of the event this responds to
  bool accepted{};///< Whether the event was accepted
  std::string message;///< Human-readable status message

  /**
   * @brief Deserializes OK message from JSON.
   *
   * @param json JSON string in format ["OK", event_id, accepted, message]
   * @return Parsed ok or std::nullopt on failure
   */
  static auto deserialize(const std::string &json) -> std::optional<ok>;
};

/**
 * @brief End of Stored Events marker.
 *
 * Sent by relays to indicate all stored events matching a subscription have been sent.
 */
struct eose
{
  std::string subscription_id;///< Subscription this EOSE applies to

  /**
   * @brief Deserializes EOSE message from JSON.
   *
   * @param json JSON string in format ["EOSE", subscription_id]
   * @return Parsed eose or std::nullopt on failure
   */
  static auto deserialize(const std::string &json) -> std::optional<eose>;
};

/**
 * @brief Nostr REQ subscription request.
 *
 * Sent to relays to subscribe to events matching filter criteria.
 */
struct req
{
  std::string subscription_id;///< Unique identifier for this subscription
  nlohmann::json filters;///< Filter criteria (NIP-01 format)

  /**
   * @brief Serializes REQ to JSON string.
   *
   * @return JSON string in format ["REQ", subscription_id, ...filters]
   */
  [[nodiscard]] auto serialize() const -> std::string;

  /**
   * @brief Deserializes REQ from JSON.
   *
   * @param json JSON string
   * @return Parsed req or std::nullopt on failure
   */
  static auto deserialize(const std::string &json) -> std::optional<req>;
};

/**
 * @brief Nostr EVENT message wrapper.
 *
 * Wraps event_data with subscription ID for relay-to-client communication.
 */
struct event
{
  std::string subscription_id;///< Subscription this event matches
  event_data data;///< The event itself

  /**
   * @brief Creates an event message from event_data.
   *
   * @param evt Event data to wrap
   * @return Constructed event message
   */
  static auto from_event_data(const event_data &evt) -> event;

  /**
   * @brief Serializes event to JSON string.
   *
   * @return JSON string in format ["EVENT", subscription_id, event_data]
   */
  [[nodiscard]] auto serialize() const -> std::string;

  /**
   * @brief Deserializes event from JSON.
   *
   * @param json JSON string
   * @return Parsed event or std::nullopt on failure
   */
  static auto deserialize(const std::string &json) -> std::optional<event>;
};

/// Maximum allowed subscription ID length
constexpr std::size_t max_subscription_id_length = 64;

/**
 * @brief Validates a subscription ID.
 *
 * @param subscription_id ID to validate
 * @throws std::invalid_argument if ID is empty or exceeds maximum length
 */
inline auto validate_subscription_id(const std::string &subscription_id) -> void
{
  if (subscription_id.empty()) { throw std::invalid_argument("Subscription ID cannot be empty"); }
  if (subscription_id.length() > max_subscription_id_length) {
    throw std::invalid_argument("Subscription ID exceeds maximum length of 64 characters");
  }
}

}// namespace radix_relay::nostr::protocol
