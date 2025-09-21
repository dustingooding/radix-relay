#pragma once

#include "signal_bridge_cxx/lib.h"
#include <cstdio>
#include <string>

namespace radix_relay {

inline auto create_node_identity() -> NodeIdentity
{
  return NodeIdentity{
    .hostname = "radix-host", .username = "radix-user", .platform = "", .mac_address = "", .install_id = ""
  };
}

inline auto get_node_fingerprint(SignalBridge &bridge) -> std::string
{
  auto identity = create_node_identity();
  return std::string(generate_node_fingerprint(bridge, identity));
}

}// namespace radix_relay
