#include <radix_relay/signal/node_identity.hpp>

namespace radix_relay::signal {

auto create_node_identity() -> NodeIdentity
{
  return NodeIdentity{
    .hostname = "radix-host", .username = "radix-user", .platform = "", .mac_address = "", .install_id = ""
  };
}

auto get_node_fingerprint(SignalBridge &bridge) -> std::string
{
  auto identity = create_node_identity();
  return std::string(generate_node_fingerprint(bridge, identity));
}

}// namespace radix_relay::signal
