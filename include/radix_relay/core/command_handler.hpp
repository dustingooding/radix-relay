#pragma once

#include <fmt/core.h>
#include <memory>
#include <radix_relay/concepts/command_handler.hpp>
#include <radix_relay/concepts/printer.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/core/default_printer.hpp>
#include <radix_relay/core/events.hpp>

#include "internal_use_only/config.hpp"

namespace radix_relay::core {

template<concepts::signal_bridge Bridge, concepts::printer Printer = default_printer> struct command_handler
{
  explicit command_handler(std::shared_ptr<Bridge> bridge,
    std::shared_ptr<Printer> printer = std::make_shared<Printer>())
    : bridge_(bridge), printer_(printer)
  {}

  template<events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

  [[nodiscard]] auto get_bridge() -> std::shared_ptr<Bridge> { return bridge_; }

private:
  auto handle_impl(const events::help & /*command*/) const -> void
  {
    std::ignore = initialized_;
    printer_->print("Interactive Commands:\n");
    printer_->print("  send <peer> <message>     Send encrypted message to peer\n");
    printer_->print("  broadcast <message>       Send to all local peers\n");
    printer_->print("  peers                     List discovered peers\n");
    printer_->print("  status                    Show network status\n");
    printer_->print("  sessions                  Show encrypted sessions\n");
    printer_->print("  mode <internet|mesh|hybrid>  Switch transport mode\n");
    printer_->print("  scan                      Force peer discovery\n");
    printer_->print("  connect <relay>           Add Nostr relay\n");
    printer_->print("  trust <peer>              Mark peer as trusted\n");
    printer_->print("  verify <peer>             Show safety numbers\n");
    printer_->print("  version                   Show version information\n");
    printer_->print("  quit                      Exit interactive mode\n");
  }

  auto handle_impl(const events::peers & /*command*/) const -> void
  {
    std::ignore = initialized_;
    printer_->print("Connected Peers: (transport layer not implemented)\n");
    printer_->print("  No peers discovered yet\n");
  }

  auto handle_impl(const events::status & /*command*/) const -> void
  {
    std::ignore = initialized_;
    printer_->print("Network Status:\n");
    printer_->print("  Internet: Not connected\n");
    printer_->print("  BLE Mesh: Not initialized\n");
    printer_->print("  Active Sessions: 0\n");

    printer_->print("\nCrypto Status:\n");
    std::string node_fingerprint = bridge_->get_node_fingerprint();
    printer_->print("  Node Fingerprint: {}\n", node_fingerprint);
  }

  auto handle_impl(const events::sessions & /*command*/) const -> void
  {
    std::ignore = initialized_;
    auto contacts = bridge_->list_contacts();

    if (contacts.empty()) {
      printer_->print("No active sessions\n");
      return;
    }

    printer_->print("Active Sessions ({}):\n", contacts.size());
    for (const auto &contact : contacts) {
      if (contact.user_alias.empty()) {
        printer_->print("  {}\n", contact.rdx_fingerprint);
      } else {
        printer_->print("  {} ({})\n", contact.user_alias, contact.rdx_fingerprint);
      }
    }
  }

  auto handle_impl(const events::scan & /*command*/) const -> void
  {
    std::ignore = initialized_;
    printer_->print("Scanning for BLE peers... (BLE transport not implemented)\n");
    printer_->print("No peers found\n");
  }

  auto handle_impl(const events::version & /*command*/) const -> void
  {
    std::ignore = initialized_;
    printer_->print("Radix Relay v{}\n", radix_relay::cmake::project_version);
  }

  auto handle_impl(const events::mode &command) const -> void
  {
    std::ignore = initialized_;
    if (command.new_mode == "internet" or command.new_mode == "mesh" or command.new_mode == "hybrid") {
      printer_->print("Switched to {} mode\n", command.new_mode);
    } else {
      printer_->print("Invalid mode. Use: internet, mesh, or hybrid\n");
    }
  }

  auto handle_impl(const events::send &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty() and not command.message.empty()) {
      printer_->print("Sending '{}' to '{}' via hybrid transport (not implemented)\n", command.message, command.peer);
    } else {
      printer_->print("Usage: send <peer> <message>\n");
    }
  }

  auto handle_impl(const events::broadcast &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.message.empty()) {
      printer_->print("Broadcasting '{}' to all local peers (not implemented)\n", command.message);
    } else {
      printer_->print("Usage: broadcast <message>\n");
    }
  }

  auto handle_impl(const events::connect &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.relay.empty()) {
      printer_->print("Connecting to Nostr relay {} (not implemented)\n", command.relay);
    } else {
      printer_->print("Usage: connect <relay>\n");
    }
  }

  auto handle_impl(const events::trust &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty()) {
      printer_->print("Marking {} as trusted (not implemented)\n", command.peer);
    } else {
      printer_->print("Usage: trust <peer>\n");
    }
  }

  auto handle_impl(const events::verify &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty()) {
      printer_->print("Safety numbers for {} (Signal Protocol not implemented)\n", command.peer);
    } else {
      printer_->print("Usage: verify <peer>\n");
    }
  }

  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<Printer> printer_;
  bool initialized_ = true;
};

}// namespace radix_relay::core
