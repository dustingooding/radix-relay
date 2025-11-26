#pragma once

#include <string>

namespace radix_relay::core {

/**
 * @brief Information about a known contact.
 */
struct contact_info
{
  std::string rdx_fingerprint;///< Signal Protocol RDX fingerprint
  std::string nostr_pubkey;///< Nostr public key
  std::string user_alias;///< User-assigned friendly name
  bool has_active_session;///< Whether an encrypted session exists
};

}// namespace radix_relay::core
