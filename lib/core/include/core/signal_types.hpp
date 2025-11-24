#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace radix_relay::signal {

struct key_maintenance_result
{
  bool signed_pre_key_rotated;
  bool kyber_pre_key_rotated;
  bool pre_keys_replenished;
};

struct decryption_result
{
  std::vector<uint8_t> plaintext;
  bool should_republish_bundle;
};

struct bundle_info
{
  std::string announcement_json;
  std::uint32_t pre_key_id;
  std::uint32_t signed_pre_key_id;
  std::uint32_t kyber_pre_key_id;
};

}// namespace radix_relay::signal
