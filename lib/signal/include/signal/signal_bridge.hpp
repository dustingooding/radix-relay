#pragma once

#include <core/contact_info.hpp>
#include <cstdint>
#include <filesystem>
#include <rust/cxx.h>
#include <signal_bridge_cxx/lib.h>
#include <signal_types/signal_types.hpp>
#include <string>
#include <vector>

namespace radix_relay::signal {

/**
 * @brief C++ wrapper for the Rust Signal Protocol implementation.
 *
 * Provides Signal Protocol operations including encryption/decryption,
 * key management, session establishment, and Nostr integration.
 */
class bridge
{
public:
  /**
   * @brief Constructs a Signal bridge with database path.
   *
   * @param bridge_db Path to Signal Protocol database
   */
  explicit bridge(const std::filesystem::path &bridge_db)
    : bridge_(radix_relay::new_signal_bridge(bridge_db.string().c_str()))
  {}

  /**
   * @brief Constructs a Signal bridge from existing Rust implementation.
   *
   * @param signal_bridge Rust SignalBridge instance
   */
  explicit bridge(rust::Box<SignalBridge> signal_bridge) : bridge_(std::move(signal_bridge)) {}

  bridge(const bridge &) = delete;
  auto operator=(const bridge &) -> bridge & = delete;
  bridge(bridge &&) = default;
  auto operator=(bridge &&) -> bridge & = default;
  ~bridge() = default;

  /**
   * @brief Returns this node's RDX fingerprint.
   *
   * @return Hex-encoded SHA-256 hash of node's identity public key
   */
  [[nodiscard]] auto get_node_fingerprint() const -> std::string;

  /**
   * @brief Lists all known contacts.
   *
   * @return Vector of contact information structs
   */
  [[nodiscard]] auto list_contacts() const -> std::vector<core::contact_info>;

  /**
   * @brief Encrypts a message for a peer.
   *
   * @param rdx Recipient's RDX fingerprint or Nostr pubkey
   * @param bytes Plaintext message bytes
   * @return Encrypted message bytes
   */
  [[nodiscard]] auto encrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const
    -> std::vector<uint8_t>;

  /**
   * @brief Decrypts an incoming message.
   *
   * @param rdx Sender's RDX fingerprint or Nostr pubkey (peer hint)
   * @param bytes Encrypted message bytes
   * @return Decryption result with plaintext and metadata
   */
  [[nodiscard]] auto decrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const
    -> decryption_result;

  /**
   * @brief Establishes a session from a prekey bundle.
   *
   * @param bundle Base64-encoded prekey bundle
   * @param alias Optional alias for the contact
   * @return RDX fingerprint of the peer
   */
  [[nodiscard]] auto add_contact_and_establish_session_from_base64(const std::string &bundle,
    const std::string &alias) const -> std::string;

  /**
   * @brief Extracts RDX fingerprint from a prekey bundle.
   *
   * @param bundle_base64 Base64-encoded prekey bundle
   * @return RDX fingerprint
   */
  [[nodiscard]] auto extract_rdx_from_bundle_base64(const std::string &bundle_base64) const -> std::string;

  /**
   * @brief Generates a signed prekey bundle announcement.
   *
   * @param version Protocol version string
   * @return Bundle information with announcement JSON and prekey IDs
   */
  [[nodiscard]] auto generate_prekey_bundle_announcement(const std::string &version) const -> bundle_info;

  /**
   * @brief Generates an empty bundle announcement for unpublishing.
   *
   * @param version Protocol version string
   * @return Signed empty bundle announcement JSON
   */
  [[nodiscard]] auto generate_empty_bundle_announcement(const std::string &version) const -> std::string;

  /**
   * @brief Assigns an alias to a contact.
   *
   * @param rdx Contact's RDX fingerprint
   * @param alias User-friendly alias
   */
  auto assign_contact_alias(const std::string &rdx, const std::string &alias) const -> void;

  /**
   * @brief Creates and signs a Nostr encrypted message event.
   *
   * @param rdx Recipient's RDX fingerprint or Nostr pubkey
   * @param content Hex-encoded encrypted content
   * @param timestamp Unix timestamp
   * @param version Protocol version string
   * @return Signed Nostr event JSON
   */
  [[nodiscard]] auto create_and_sign_encrypted_message(const std::string &rdx,
    const std::string &content,
    uint32_t timestamp,
    const std::string &version) const -> std::string;

  /**
   * @brief Looks up a contact by RDX fingerprint or alias.
   *
   * @param alias Contact identifier (RDX fingerprint or user alias)
   * @return Contact information
   * @throws std::runtime_error if contact not found
   */
  [[nodiscard]] auto lookup_contact(const std::string &alias) const -> core::contact_info;

  /**
   * @brief Signs a Nostr event with node's private key.
   *
   * @param event_json Unsigned event JSON
   * @return Signed event JSON with id and sig fields
   */
  [[nodiscard]] auto sign_nostr_event(const std::string &event_json) const -> std::string;

  /**
   * @brief Creates a Nostr subscription filter for messages to this node.
   *
   * @param subscription_id Subscription identifier
   * @param since_timestamp Optional timestamp to filter messages since
   * @return REQ message JSON
   */
  [[nodiscard]] auto create_subscription_for_self(const std::string &subscription_id,
    std::uint64_t since_timestamp = 0) const -> std::string;

  /**
   * @brief Updates the timestamp of the last received message.
   *
   * @param timestamp Unix timestamp
   */
  auto update_last_message_timestamp(std::uint64_t timestamp) const -> void;

  /**
   * @brief Performs periodic key maintenance and rotation.
   *
   * @return Result indicating which keys were rotated
   */
  [[nodiscard]] auto perform_key_maintenance() const -> key_maintenance_result;

  /**
   * @brief Records published prekey bundle to track used keys.
   *
   * @param pre_key_id One-time prekey ID
   * @param signed_pre_key_id Signed prekey ID
   * @param kyber_pre_key_id Kyber prekey ID
   */
  auto record_published_bundle(std::uint32_t pre_key_id,
    std::uint32_t signed_pre_key_id,
    std::uint32_t kyber_pre_key_id) const -> void;

  /**
   * @brief Retrieves all conversations ordered by most recent message.
   *
   * @param include_archived Whether to include archived conversations
   * @return Vector of conversations with metadata
   */
  [[nodiscard]] auto get_conversations(bool include_archived) const -> std::vector<conversation>;

  /**
   * @brief Retrieves messages from a conversation with pagination.
   *
   * @param rdx_fingerprint Contact's RDX fingerprint
   * @param limit Maximum number of messages to retrieve
   * @param offset Number of messages to skip (for pagination)
   * @return Vector of stored messages ordered newest first
   */
  [[nodiscard]] auto get_conversation_messages(const std::string &rdx_fingerprint,
    std::uint32_t limit,
    std::uint32_t offset) const -> std::vector<stored_message>;

  /**
   * @brief Marks all messages in a conversation as read.
   *
   * @param rdx_fingerprint Contact's RDX fingerprint
   */
  auto mark_conversation_read(const std::string &rdx_fingerprint) const -> void;

  /**
   * @brief Marks messages as read up to a specific timestamp.
   *
   * This prevents race conditions where new messages arrive after loading history.
   * Only marks messages with timestamp <= up_to_timestamp as read.
   *
   * @param rdx_fingerprint Contact's RDX fingerprint
   * @param up_to_timestamp Messages up to and including this timestamp are marked read
   */
  auto mark_conversation_read_up_to(const std::string &rdx_fingerprint, std::uint64_t up_to_timestamp) const -> void;

  /**
   * @brief Deletes a specific message from history.
   *
   * @param message_id Database ID of the message to delete
   */
  auto delete_message(std::int64_t message_id) const -> void;

  /**
   * @brief Deletes an entire conversation and all its messages.
   *
   * @param rdx_fingerprint Contact's RDX fingerprint
   */
  auto delete_conversation(const std::string &rdx_fingerprint) const -> void;

  /**
   * @brief Gets the number of unread messages in a conversation.
   *
   * @param rdx_fingerprint Contact's RDX fingerprint
   * @return Number of unread messages
   */
  [[nodiscard]] auto get_unread_count(const std::string &rdx_fingerprint) const -> std::uint32_t;

private:
  mutable rust::Box<SignalBridge> bridge_;
};


}// namespace radix_relay::signal
