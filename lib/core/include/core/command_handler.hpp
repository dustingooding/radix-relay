#pragma once

#include <async/async_queue.hpp>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <core/overload.hpp>
#include <fmt/core.h>
#include <memory>
#include <platform/time_utils.hpp>

#include "internal_use_only/config.hpp"

namespace radix_relay::core {

template<concepts::signal_bridge Bridge> struct command_handler_context
{
  using display_queue_t = std::shared_ptr<async::async_queue<events::display_filter_input_t>>;
  using transport_queue_t = std::shared_ptr<async::async_queue<events::transport::in_t>>;
  using session_queue_t = std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>>;
  using connection_monitor_queue_t = std::shared_ptr<async::async_queue<events::connection_monitor::in_t>>;

  std::shared_ptr<Bridge> bridge;
  display_queue_t display_queue;
  transport_queue_t transport_queue;
  session_queue_t session_queue;
  connection_monitor_queue_t connection_monitor_queue;

  template<typename... Args> auto emit(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    display_queue->push(events::display_message{ .message = fmt::format(format_string, std::forward<Args>(args)...),
      .contact_rdx = std::nullopt,
      .timestamp = platform::current_timestamp_ms(),
      .source_type = events::display_message::source::command_feedback });
  }
};

template<concepts::signal_bridge Bridge>
auto make_command_handler(std::shared_ptr<Bridge> bridge,
  std::shared_ptr<async::async_queue<events::display_filter_input_t>> display_queue,
  std::shared_ptr<async::async_queue<events::transport::in_t>> transport_queue,
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> session_queue,
  std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> connection_monitor_queue)
{
  auto ctx = std::make_shared<command_handler_context<Bridge>>(command_handler_context<Bridge>{
    .bridge = std::move(bridge),
    .display_queue = std::move(display_queue),
    .transport_queue = std::move(transport_queue),
    .session_queue = std::move(session_queue),
    .connection_monitor_queue = std::move(connection_monitor_queue),
  });

  return overload{
    [ctx](const events::help &) {
      ctx->emit(
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
    },

    [ctx](const events::peers &) {
      ctx->emit(
        "Connected Peers: (transport layer not implemented)\n"
        "  No peers discovered yet\n");
    },

    [ctx](const events::status &) {
      ctx->connection_monitor_queue->push(events::connection_monitor::query_status{});
      std::string node_fingerprint = ctx->bridge->get_node_fingerprint();
      ctx->emit("\nCrypto Status:\n  Node Fingerprint: {}\n", node_fingerprint);
    },

    [ctx](const events::sessions &) {
      auto contacts = ctx->bridge->list_contacts();

      if (contacts.empty()) {
        ctx->emit("No active sessions\n");
        return;
      }

      ctx->emit("Active Sessions ({}):\n", contacts.size());
      for (const auto &contact : contacts) {
        if (contact.user_alias.empty()) {
          ctx->emit("  {}\n", contact.rdx_fingerprint);
        } else {
          ctx->emit("  {} ({})\n", contact.user_alias, contact.rdx_fingerprint);
        }
      }
    },

    [ctx](const events::identities &) { ctx->session_queue->push(events::list_identities{}); },

    [ctx](const events::publish_identity &) {
      ctx->session_queue->push(events::publish_identity{});
      ctx->emit("Publishing identity to network...\n");
    },

    [ctx](const events::unpublish_identity &) {
      ctx->session_queue->push(events::unpublish_identity{});
      ctx->emit("Unpublishing identity from network...\n");
    },

    [ctx](const events::scan &) {
      ctx->emit(
        "Scanning for BLE peers... (BLE transport not implemented)\n"
        "  No peers found\n");
    },

    [ctx](const events::version &) { ctx->emit("Radix Relay v{}\n", radix_relay::cmake::project_version); },

    [ctx](const events::mode &command) {
      if (command.new_mode == "internet" or command.new_mode == "mesh" or command.new_mode == "hybrid") {
        ctx->emit("Switched to {} mode\n", command.new_mode);
      } else {
        ctx->emit("Invalid mode. Use: internet, mesh, or hybrid\n");
      }
    },

    [ctx](const events::send &command) {
      if (not command.peer.empty() and not command.message.empty()) {
        ctx->session_queue->push(command);
        ctx->emit("Sending '{}' to '{}'...\n", command.message, command.peer);
      } else {
        ctx->emit("Usage: send <peer> <message>\n");
      }
    },

    [ctx](const events::broadcast &command) {
      if (not command.message.empty()) {
        ctx->emit("Broadcasting '{}' to all local peers (not implemented)\n", command.message);
      } else {
        ctx->emit("Usage: broadcast <message>\n");
      }
    },

    [ctx](const events::connect &command) {
      if (not command.relay.empty()) {
        ctx->session_queue->push(command);
        ctx->emit("Connecting to Nostr relay {}\n", command.relay);
      } else {
        ctx->emit("Usage: connect <relay>\n");
      }
    },

    [ctx](const events::disconnect &) {
      ctx->transport_queue->push(events::transport::disconnect{});
      ctx->emit("Disconnecting from Nostr relay\n");
    },

    [ctx](const events::trust &command) {
      if (not command.peer.empty()) {
        ctx->session_queue->push(command);
        ctx->emit("Establishing session with {}...\n", command.peer);
      } else {
        ctx->emit("Usage: trust <peer> [alias]\n");
      }
    },

    [ctx](const events::verify &command) {
      if (not command.peer.empty()) {
        ctx->emit("Safety numbers for {} (Signal Protocol not implemented)\n", command.peer);
      } else {
        ctx->emit("Usage: verify <peer>\n");
      }
    },

    [ctx](const events::chat &command) {
      if (command.contact.empty()) {
        ctx->emit("Usage: /chat <contact>\n");
        return;
      }

      try {
        const auto contact = ctx->bridge->lookup_contact(command.contact);
        const auto display_name = contact.user_alias.empty() ? contact.rdx_fingerprint : contact.user_alias;

        ctx->display_queue->push(
          events::enter_chat_mode{ .rdx_fingerprint = contact.rdx_fingerprint, .display_name = display_name });

        constexpr std::uint32_t history_limit = 5;
        const auto messages = ctx->bridge->get_conversation_messages(contact.rdx_fingerprint, history_limit, 0);

        if (not messages.empty()) {
          ctx->display_queue->push(events::display_message{
            .message = fmt::format("--- Conversation History ({} messages) ---", messages.size()),
            .contact_rdx = contact.rdx_fingerprint,
            .timestamp = platform::current_timestamp_ms(),
            .source_type = events::display_message::source::system });

          for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            const auto &msg = *it;

            const auto direction_indicator = (msg.direction == signal::MessageDirection::Incoming) ? "← " : "→ ";
            const auto sender_name = (msg.direction == signal::MessageDirection::Incoming) ? display_name : "You";

            const auto formatted_message = fmt::format("{}{}: {}", direction_indicator, sender_name, msg.content);

            const auto source_type = (msg.direction == signal::MessageDirection::Incoming)
                                       ? events::display_message::source::incoming_message
                                       : events::display_message::source::outgoing_message;

            ctx->display_queue->push(events::display_message{ .message = formatted_message,
              .contact_rdx = contact.rdx_fingerprint,
              .timestamp = msg.timestamp,
              .source_type = source_type });
          }

          ctx->display_queue->push(events::display_message{ .message = "--- End of History ---",
            .contact_rdx = contact.rdx_fingerprint,
            .timestamp = platform::current_timestamp_ms(),
            .source_type = events::display_message::source::system });

          const auto newest_timestamp = messages.front().timestamp;
          ctx->bridge->mark_conversation_read_up_to(contact.rdx_fingerprint, newest_timestamp);
        } else {
          ctx->bridge->mark_conversation_read(contact.rdx_fingerprint);
        }

        ctx->emit("Entering chat with {}\n", display_name);
      } catch (const std::exception &) {
        ctx->emit("Contact not found: {}\n", command.contact);
      }
    },

    [ctx](const events::leave &) {
      ctx->display_queue->push(events::exit_chat_mode{});
      ctx->emit("Exiting chat mode\n");
    },

    [](const events::unknown_command & /*command*/) {
      // No-op: unknown commands are silently ignored
    },
  };
}

template<concepts::signal_bridge Bridge>
using command_handler = decltype(make_command_handler(std::declval<std::shared_ptr<Bridge>>(),
  std::declval<std::shared_ptr<async::async_queue<events::display_filter_input_t>>>(),
  std::declval<std::shared_ptr<async::async_queue<events::transport::in_t>>>(),
  std::declval<std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>>>(),
  std::declval<std::shared_ptr<async::async_queue<events::connection_monitor::in_t>>>()));

}// namespace radix_relay::core
