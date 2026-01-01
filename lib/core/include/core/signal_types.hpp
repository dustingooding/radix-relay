#pragma once

#include <cstdint>
#include <signal_bridge_cxx/lib.h>
#include <string>
#include <vector>

namespace radix_relay::signal {

// Re-export CXX-generated enums for convenience
using MessageDirection = radix_relay::MessageDirection;
using MessageType = radix_relay::MessageType;
using DeliveryStatus = radix_relay::DeliveryStatus;

/**
 * @brief Result of performing key maintenance operations.
 */
struct key_maintenance_result
{
  bool signed_pre_key_rotated;///< Whether the signed prekey was rotated
  bool kyber_pre_key_rotated;///< Whether the Kyber PQ prekey was rotated
  bool pre_keys_replenished;///< Whether one-time prekeys were replenished
};

/**
 * @brief Result of decrypting a received message.
 */
struct decryption_result
{
  std::vector<uint8_t> plaintext;///< Decrypted message content
  bool should_republish_bundle;///< Whether sender exhausted our prekeys
};

/**
 * @brief Information about a generated prekey bundle.
 */
struct bundle_info
{
  std::string announcement_json;///< JSON-formatted bundle announcement
  std::uint32_t pre_key_id;///< One-time prekey ID included in bundle
  std::uint32_t signed_pre_key_id;///< Signed prekey ID included in bundle
  std::uint32_t kyber_pre_key_id;///< Kyber PQ prekey ID included in bundle
};

/**
 * @brief A stored message from history.
 */
struct stored_message
{
  std::int64_t id;///< Database ID of the message
  std::int64_t conversation_id;///< ID of the conversation this message belongs to
  MessageDirection direction;///< Message direction (Incoming or Outgoing)
  std::uint64_t timestamp;///< Message timestamp (milliseconds since epoch)
  MessageType message_type;///< Message type (Text, BundleAnnouncement, or System)
  std::string content;///< Message content (plaintext, DB encrypted by SQLCipher)
  DeliveryStatus delivery_status;///< Delivery status for outgoing messages
  bool was_prekey_message;///< Whether this was a PreKey message
  bool session_established;///< Whether session was established with this message
};

/**
 * @brief A conversation/thread with a contact.
 */
struct conversation
{
  std::int64_t id;///< Database ID of the conversation
  std::string rdx_fingerprint;///< Contact's RDX fingerprint
  std::uint64_t last_message_timestamp;///< Timestamp of most recent message
  std::uint32_t unread_count;///< Number of unread messages
  bool archived;///< Whether conversation is archived
};

}// namespace radix_relay::signal
