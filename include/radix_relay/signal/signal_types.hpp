#pragma once

#include <string>

namespace radix_relay::signal {

struct contact_info
{
  std::string rdx_fingerprint;
  std::string nostr_pubkey;
  std::string user_alias;
  bool has_active_session;
};

}// namespace radix_relay::signal
