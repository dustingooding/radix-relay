#pragma once

#include "signal_bridge_cxx/lib.h"
#include <string>

namespace radix_relay::signal {

[[nodiscard]] auto create_node_identity() -> NodeIdentity;

[[nodiscard]] auto get_node_fingerprint(SignalBridge &bridge) -> std::string;

}// namespace radix_relay::signal
