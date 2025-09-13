#pragma once

#include "crypto_utils_cxx/lib.h"
#include <string>

namespace radix_relay {

inline auto create_node_identity() -> NodeIdentity
{
  return NodeIdentity{ .hostname = "radix-host",
    .username = "radix-user",
    .platform = "",
    .machine_id = "",
    .mac_address = "",
    .install_id = "" };
}

inline auto get_node_fingerprint() -> std::string
{
  auto identity = create_node_identity();
  return std::string(generate_node_fingerprint(identity));
}

}// namespace radix_relay
