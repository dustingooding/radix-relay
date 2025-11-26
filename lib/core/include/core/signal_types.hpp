#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace radix_relay::signal {

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

}// namespace radix_relay::signal
