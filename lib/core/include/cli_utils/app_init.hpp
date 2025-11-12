#pragma once

#include <async/async_queue.hpp>
#include <cli_utils/cli_parser.hpp>
#include <cli_utils/tui_sink.hpp>
#include <concepts/command_handler.hpp>
#include <core/events.hpp>
#include <core/standard_event_handler.hpp>
#include <fmt/core.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

namespace radix_relay::cli_utils {

struct app_state
{
  std::string node_fingerprint;
  std::string mode;
  std::string identity_path;
};

inline auto configure_logging(const cli_args &args,
  std::shared_ptr<async::async_queue<core::events::display_message>> display_queue = nullptr) -> void
{
  if (display_queue) {
    auto tui_sink = std::make_shared<tui_sink_mutex_t>(display_queue);
    auto logger = std::make_shared<spdlog::logger>("tui_logger", tui_sink);
    spdlog::set_default_logger(logger);
  }

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
[[nodiscard]] inline auto execute_cli_command(const cli_args &args, std::shared_ptr<CmdHandler> command_handler) -> bool
{

  if (args.show_version) {
    command_handler->handle(radix_relay::core::events::version{});
    return true;
  }

  if (args.send_parsed) {
    command_handler->handle(
      radix_relay::core::events::send{ .peer = args.send_recipient, .message = args.send_message });
    return true;
  }

  if (args.peers_parsed) {
    command_handler->handle(radix_relay::core::events::peers{});
    return true;
  }

  if (args.status_parsed) {
    command_handler->handle(radix_relay::core::events::status{});
    return true;
  }

  return false;
}

}// namespace radix_relay::cli_utils
