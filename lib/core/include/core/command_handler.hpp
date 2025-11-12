#pragma once

#include <async/async_queue.hpp>
#include <concepts/command_handler.hpp>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <fmt/core.h>
#include <memory>

#include "internal_use_only/config.hpp"

namespace radix_relay::core {

template<concepts::signal_bridge Bridge> struct command_handler
{
  explicit command_handler(const std::shared_ptr<Bridge> &bridge,
    const std::shared_ptr<async::async_queue<events::display_message>> &display_out_queue,
    const std::shared_ptr<async::async_queue<events::transport::in_t>> &transport_out_queue,
    const std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> &session_out_queue)
    : bridge_(bridge), display_out_queue_(display_out_queue), transport_out_queue_(transport_out_queue),
      session_out_queue_(session_out_queue)
  {}

  template<events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

  [[nodiscard]] auto get_bridge() -> std::shared_ptr<Bridge> { return bridge_; }

private:
  template<typename... Args> auto emit(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    display_out_queue_->push(events::display_message{ fmt::format(format_string, std::forward<Args>(args)...) });
  }

  auto handle_impl(const events::help & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit("Interactive Commands:\n");
    emit("  send <peer> <message>     Send encrypted message to peer\n");
    emit("  broadcast <message>       Send to all local peers\n");
    emit("  peers                     List discovered peers\n");
    emit("  status                    Show network status\n");
    emit("  sessions                  Show encrypted sessions\n");
    emit("  mode <internet|mesh|hybrid>  Switch transport mode\n");
    emit("  scan                      Force peer discovery\n");
    emit("  connect <relay>           Add Nostr relay\n");
    emit("  disconnect                Disconnect from Nostr relay\n");
    emit("  trust <peer>              Mark peer as trusted\n");
    emit("  verify <peer>             Show safety numbers\n");
    emit("  version                   Show version information\n");
    emit("  quit                      Exit interactive mode\n");
  }

  auto handle_impl(const events::peers & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit("Connected Peers: (transport layer not implemented)\n");
    emit("  No peers discovered yet\n");
  }

  auto handle_impl(const events::status & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit("Network Status:\n");
    emit("  Internet: Not connected\n");
    emit("  BLE Mesh: Not initialized\n");
    emit("  Active Sessions: 0\n");

    emit("\nCrypto Status:\n");
    std::string node_fingerprint = bridge_->get_node_fingerprint();
    emit("  Node Fingerprint: {}\n", node_fingerprint);
  }

  auto handle_impl(const events::sessions & /*command*/) const -> void
  {
    std::ignore = initialized_;
    auto contacts = bridge_->list_contacts();

    if (contacts.empty()) {
      emit("No active sessions\n");
      return;
    }

    emit("Active Sessions ({}):\n", contacts.size());
    for (const auto &contact : contacts) {
      if (contact.user_alias.empty()) {
        emit("  {}\n", contact.rdx_fingerprint);
      } else {
        emit("  {} ({})\n", contact.user_alias, contact.rdx_fingerprint);
      }
    }
  }

  auto handle_impl(const events::scan & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit("Scanning for BLE peers... (BLE transport not implemented)\n");
    emit("No peers found\n");
  }

  auto handle_impl(const events::version & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit("Radix Relay v{}\n", radix_relay::cmake::project_version);
  }

  auto handle_impl(const events::mode &command) const -> void
  {
    std::ignore = initialized_;
    if (command.new_mode == "internet" or command.new_mode == "mesh" or command.new_mode == "hybrid") {
      emit("Switched to {} mode\n", command.new_mode);
    } else {
      emit("Invalid mode. Use: internet, mesh, or hybrid\n");
    }
  }

  auto handle_impl(const events::send &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty() and not command.message.empty()) {
      emit("Sending '{}' to '{}' via hybrid transport (not implemented)\n", command.message, command.peer);
    } else {
      emit("Usage: send <peer> <message>\n");
    }
  }

  auto handle_impl(const events::broadcast &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.message.empty()) {
      emit("Broadcasting '{}' to all local peers (not implemented)\n", command.message);
    } else {
      emit("Usage: broadcast <message>\n");
    }
  }

  auto handle_impl(const events::connect &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.relay.empty()) {
      session_out_queue_->push(command);
      emit("Connecting to Nostr relay {}\n", command.relay);
    } else {
      emit("Usage: connect <relay>\n");
    }
  }

  auto handle_impl(const events::disconnect & /*command*/) const -> void
  {
    std::ignore = initialized_;
    transport_out_queue_->push(events::transport::disconnect{});
    emit("Disconnecting from Nostr relay\n");
  }

  auto handle_impl(const events::trust &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty()) {
      emit("Marking {} as trusted (not implemented)\n", command.peer);
    } else {
      emit("Usage: trust <peer>\n");
    }
  }

  auto handle_impl(const events::verify &command) const -> void
  {
    std::ignore = initialized_;
    if (not command.peer.empty()) {
      emit("Safety numbers for {} (Signal Protocol not implemented)\n", command.peer);
    } else {
      emit("Usage: verify <peer>\n");
    }
  }

  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<events::display_message>> display_out_queue_;
  std::shared_ptr<async::async_queue<events::transport::in_t>> transport_out_queue_;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> session_out_queue_;
  bool initialized_ = true;
};

}// namespace radix_relay::core
