#pragma once

#include <async/async_queue.hpp>
#include <concepts/command_handler.hpp>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <fmt/core.h>
#include <memory>
#include <platform/time_utils.hpp>

#include "internal_use_only/config.hpp"

namespace radix_relay::core {

/**
 * @brief Handles typed command events and coordinates with subsystems.
 *
 * @tparam Bridge Type satisfying the signal_bridge concept
 *
 * Routes commands to appropriate subsystems (display, transport, session orchestrator)
 * and interacts with the Signal Protocol bridge for cryptographic operations.
 */
template<concepts::signal_bridge Bridge> struct command_handler
{
  // Type traits for standard_processor
  using in_queue_t = async::async_queue<events::raw_command>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::display_filter_input_t>> display;
    std::shared_ptr<async::async_queue<events::transport::in_t>> transport;
    std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> session;
    std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> connection_monitor;
  };

  /**
   * @brief Constructs a command handler with required subsystem queues.
   *
   * @param bridge Signal Protocol bridge for crypto operations
   * @param queues Output queues for subsystems
   */
  explicit command_handler(const std::shared_ptr<Bridge> &bridge, const out_queues_t &queues)
    : bridge_(bridge), display_out_queue_(queues.display), transport_out_queue_(queues.transport),
      session_out_queue_(queues.session), connection_monitor_out_queue_(queues.connection_monitor)
  {}

  /**
   * @brief Handles a typed command event.
   *
   * @tparam T Command type satisfying the Command concept
   * @param command The command to handle
   */
  template<events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

  /**
   * @brief Returns the Signal Protocol bridge.
   *
   * @return Shared pointer to the bridge
   */
  [[nodiscard]] auto get_bridge() -> std::shared_ptr<Bridge> { return bridge_; }

private:
  template<typename... Args> auto emit(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    display_out_queue_->push(
      events::display_message{ .message = fmt::format(format_string, std::forward<Args>(args)...),
        .contact_rdx = std::nullopt,
        .timestamp = platform::current_timestamp_ms(),
        .source_type = events::display_message::source::command_feedback });
  }

  auto handle_impl(const events::help & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit(
      "Interactive Commands:\n"
      "  /broadcast <message>          Send to all local peers\n"
      "  /chat <contact>               Enter chat mode with contact\n"
      "  /connect <relay>              Add Nostr relay\n"
      "  /disconnect                   Disconnect from Nostr relay\n"
      "  /identities                   List discovered identities\n"
      "  /leave                        Exit chat mode\n"
      "  /mode <internet|mesh|hybrid>  Switch transport mode\n"
      "  /peers                        List discovered peers\n"
      "  /publish                      Publish identity to network\n"
      "  /scan                         Force peer discovery\n"
      "  /send <peer> <message>        Send encrypted message to peer\n"
      "  /sessions                     Show encrypted sessions\n"
      "  /status                       Show network status\n"
      "  /trust <peer> [alias]         Establish session with peer\n"
      "  /verify <peer>                Show safety numbers\n"
      "  /version                      Show version information\n"
      "  /quit                         Exit interactive mode\n");
  }

  auto handle_impl(const events::peers & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit(
      "Connected Peers: (transport layer not implemented)\n"
      "  No peers discovered yet\n");
  }

  auto handle_impl(const events::status & /*command*/) const -> void
  {
    std::ignore = initialized_;

    connection_monitor_out_queue_->push(events::connection_monitor::query_status{});

    std::string node_fingerprint = bridge_->get_node_fingerprint();
    emit("\nCrypto Status:\n  Node Fingerprint: {}\n", node_fingerprint);
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

  auto handle_impl(const events::identities & /*command*/) const -> void
  {
    std::ignore = initialized_;
    session_out_queue_->push(events::list_identities{});
  }

  auto handle_impl(const events::publish_identity & /*command*/) const -> void
  {
    std::ignore = initialized_;
    session_out_queue_->push(events::publish_identity{});
    emit("Publishing identity to network...\n");
  }

  auto handle_impl(const events::unpublish_identity & /*command*/) const -> void
  {
    std::ignore = initialized_;
    session_out_queue_->push(events::unpublish_identity{});
    emit("Unpublishing identity from network...\n");
  }

  auto handle_impl(const events::scan & /*command*/) const -> void
  {
    std::ignore = initialized_;
    emit(
      "Scanning for BLE peers... (BLE transport not implemented)\n"
      "  No peers found\n");
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
      session_out_queue_->push(command);
      emit("Sending '{}' to '{}'...\n", command.message, command.peer);
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
      session_out_queue_->push(command);
      emit("Establishing session with {}...\n", command.peer);
    } else {
      emit("Usage: trust <peer> [alias]\n");
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

  auto handle_impl(const events::chat &command) const -> void
  {
    std::ignore = initialized_;
    if (command.contact.empty()) {
      emit("Usage: /chat <contact>\n");
      return;
    }

    try {
      const auto contact = bridge_->lookup_contact(command.contact);
      display_out_queue_->push(events::enter_chat_mode{ .rdx_fingerprint = contact.rdx_fingerprint });
      const auto display_name = contact.user_alias.empty() ? contact.rdx_fingerprint : contact.user_alias;
      emit("Entering chat with {} ({})\n", display_name, contact.rdx_fingerprint);
    } catch (const std::exception & /*e*/) {
      emit("Contact not found: {}\n", command.contact);
    }
  }

  auto handle_impl(const events::leave & /*command*/) const -> void
  {
    std::ignore = initialized_;
    display_out_queue_->push(events::exit_chat_mode{});
    emit("Exiting chat mode\n");
  }

  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<events::display_filter_input_t>> display_out_queue_;
  std::shared_ptr<async::async_queue<events::transport::in_t>> transport_out_queue_;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> session_out_queue_;
  std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> connection_monitor_out_queue_;
  bool initialized_ = true;
};

}// namespace radix_relay::core
