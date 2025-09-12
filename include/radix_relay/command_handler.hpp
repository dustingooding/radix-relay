#pragma once

#include <fmt/core.h>
#include <radix_relay/concepts/command_handler.hpp>
#include <radix_relay/events/events.hpp>
#include <utility>

// Include generated CXX bridge header for crypto utilities
#include "crypto_utils_cxx/lib.h"

#include "internal_use_only/config.hpp"

namespace radix_relay {

struct CommandHandler
{
  template<events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

private:
  auto handle_impl(const events::help & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Interactive Commands:\n");
    fmt::print("  send <peer> <message>     Send encrypted message to peer\n");
    fmt::print("  broadcast <message>       Send to all local peers\n");
    fmt::print("  peers                     List discovered peers\n");
    fmt::print("  status                    Show network status\n");
    fmt::print("  sessions                  Show encrypted sessions\n");
    fmt::print("  mode <internet|mesh|hybrid>  Switch transport mode\n");
    fmt::print("  scan                      Force peer discovery\n");
    fmt::print("  connect <relay>           Add Nostr relay\n");
    fmt::print("  trust <peer>              Mark peer as trusted\n");
    fmt::print("  verify <peer>             Show safety numbers\n");
    fmt::print("  version                   Show version information\n");
    fmt::print("  quit                      Exit interactive mode\n");
  }

  auto handle_impl(const events::peers & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Connected Peers: (transport layer not implemented)\n");
    fmt::print("  No peers discovered yet\n");
  }

  auto handle_impl(const events::status & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Network Status:\n");
    fmt::print("  Internet: Not connected\n");
    fmt::print("  BLE Mesh: Not initialized\n");
    fmt::print("  Active Sessions: 0\n");

    // Crypto Status - call to Rust crypto utilities
    fmt::print("\nCrypto Status:\n");
    std::string node_fingerprint = std::string(radix_relay::get_node_identity_fingerprint());
    fmt::print("  Node Fingerprint: {}\n", node_fingerprint);
  }

  auto handle_impl(const events::sessions & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Active Encrypted Sessions: (Signal Protocol not implemented)\n");
    fmt::print("  No active sessions\n");
  }

  auto handle_impl(const events::scan & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Scanning for BLE peers... (BLE transport not implemented)\n");
    fmt::print("No peers found\n");
  }

  auto handle_impl(const events::version & /*command*/) const -> void
  {
    std::ignore = initialized_;
    fmt::print("Radix Relay v{}\n", radix_relay::cmake::project_version);
  }

  auto handle_impl(const events::mode &command) const -> void
  {
    std::ignore = initialized_;
    if (command.new_mode == "internet" || command.new_mode == "mesh" || command.new_mode == "hybrid") {
      fmt::print("Switched to {} mode\n", command.new_mode);
    } else {
      fmt::print("Invalid mode. Use: internet, mesh, or hybrid\n");
    }
  }

  auto handle_impl(const events::send &command) const -> void
  {
    std::ignore = initialized_;
    if (!command.peer.empty() && !command.message.empty()) {
      fmt::print("Sending '{}' to '{}' via hybrid transport (not implemented)\n", command.message, command.peer);
    } else {
      fmt::print("Usage: send <peer> <message>\n");
    }
  }

  auto handle_impl(const events::broadcast &command) const -> void
  {
    std::ignore = initialized_;
    if (!command.message.empty()) {
      fmt::print("Broadcasting '{}' to all local peers (not implemented)\n", command.message);
    } else {
      fmt::print("Usage: broadcast <message>\n");
    }
  }

  auto handle_impl(const events::connect &command) const -> void
  {
    std::ignore = initialized_;
    if (!command.relay.empty()) {
      fmt::print("Connecting to Nostr relay {} (not implemented)\n", command.relay);
    } else {
      fmt::print("Usage: connect <relay>\n");
    }
  }

  auto handle_impl(const events::trust &command) const -> void
  {
    std::ignore = initialized_;
    if (!command.peer.empty()) {
      fmt::print("Marking {} as trusted (not implemented)\n", command.peer);
    } else {
      fmt::print("Usage: trust <peer>\n");
    }
  }

  auto handle_impl(const events::verify &command) const -> void
  {
    std::ignore = initialized_;
    if (!command.peer.empty()) {
      fmt::print("Safety numbers for {} (Signal Protocol not implemented)\n", command.peer);
    } else {
      fmt::print("Usage: verify <peer>\n");
    }
  }

private:
  bool initialized_ = true;
};

static_assert(concepts::CommandHandler<CommandHandler>);

}// namespace radix_relay
