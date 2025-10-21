#pragma once

#include <fmt/core.h>
#include <memory>
#include <radix_relay/concepts/command_handler.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/events/events.hpp>

#include "internal_use_only/config.hpp"

namespace radix_relay {

template<concepts::signal_bridge Bridge> struct command_handler
{
  explicit command_handler(std::shared_ptr<Bridge> bridge) : bridge_(bridge) {}

  template<events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

  auto get_bridge() -> std::shared_ptr<Bridge> { return bridge_; }

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

    fmt::print("\nCrypto Status:\n");
    std::string node_fingerprint = bridge_->get_node_fingerprint();
    fmt::print("  Node Fingerprint: {}\n", node_fingerprint);
  }

  auto handle_impl(const events::sessions & /*command*/) const -> void
  {
    std::ignore = initialized_;
    auto contacts = bridge_->list_contacts();

    if (contacts.empty()) {
      fmt::print("No active sessions\n");
      return;
    }

    fmt::print("Active Sessions ({}):\n", contacts.size());
    for (const auto &contact : contacts) {
      if (contact.user_alias.empty()) {
        fmt::print("  {}\n", contact.rdx_fingerprint);
      } else {
        fmt::print("  {} ({})\n", contact.user_alias, contact.rdx_fingerprint);
      }
    }
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

  std::shared_ptr<Bridge> bridge_;
  bool initialized_ = true;
};

}// namespace radix_relay
