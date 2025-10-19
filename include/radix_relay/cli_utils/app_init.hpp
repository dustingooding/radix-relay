#pragma once

#include <fmt/core.h>
#include <memory>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/concepts/command_handler.hpp>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace radix_relay {

struct app_state
{
  std::string node_fingerprint;
  std::string mode;
  std::string identity_path;
};

inline auto configure_logging(const cli_args &args) -> void
{
  if (args.verbose) { spdlog::set_level(spdlog::level::debug); }
}


inline auto print_app_banner(const app_state &state) -> void
{
  fmt::print("Radix Relay v{} - Interactive Mode\n", radix_relay::cmake::project_version);
  fmt::print("Node: {} ({})\n", state.node_fingerprint, state.identity_path);
  fmt::print("transport: {}\n", state.mode);
  fmt::print("Connected Peers: 0 (transport layer not implemented)\n\n");
}

inline auto print_available_commands() -> void
{
  fmt::print(
    "Available commands: send, broadcast, peers, status, sessions, mode, scan, connect, trust, verify, version, help, "
    "quit\n\n");
}

template<concepts::command_handler CmdHandler>
inline auto execute_cli_command(const cli_args &args, std::shared_ptr<CmdHandler> command_handler) -> bool
{

  if (args.show_version) {
    command_handler->handle(radix_relay::events::version{});
    return true;
  }

  if (args.send_parsed) {
    command_handler->handle(radix_relay::events::send{ .peer = args.send_recipient, .message = args.send_message });
    return true;
  }

  if (args.peers_parsed) {
    command_handler->handle(radix_relay::events::peers{});
    return true;
  }

  if (args.status_parsed) {
    command_handler->handle(radix_relay::events::status{});
    return true;
  }

  return false;
}

}// namespace radix_relay
