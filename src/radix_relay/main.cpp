#include <cstddef>
#include <string>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <radix_relay/cli.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "internal_use_only/config.hpp"

// Include generated CXX bridge header for crypto utilities
#include "crypto_utils_cxx/lib.h"

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  CLI::App app{ "Radix Relay - Hybrid Mesh Communications", "radix-relay" };

  std::string identity_path = "~/.radix/identity.key";
  std::string mode = "hybrid";
  bool verbose = false;
  bool show_version = false;

  app.add_option("-i,--identity", identity_path, "Path to identity key file");
  app.add_option("-m,--mode", mode, "Transport mode: internet, mesh, hybrid")
    ->check(CLI::IsMember({ "internet", "mesh", "hybrid" }));
  app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
  app.add_flag("--version", show_version, "Show version information");

  auto *send_cmd = app.add_subcommand("send", "Send a message");
  std::string recipient;
  std::string send_message;
  send_cmd->add_option("recipient", recipient, "Node ID or contact name")->required();
  send_cmd->add_option("message", send_message, "Message content")->required();

  auto *peers_cmd = app.add_subcommand("peers", "List discovered peers");

  auto *status_cmd = app.add_subcommand("status", "Show network status");

  CLI11_PARSE(app, argc, argv);

  const radix_relay::StandardEventHandler::command_handler_t command_handler;

  if (show_version) {
    command_handler.handle(radix_relay::events::version{});
    return 0;
  }

  if (verbose) { spdlog::set_level(spdlog::level::debug); }

  spdlog::info("Radix Relay {} starting", radix_relay::cmake::project_version);
  spdlog::info("Identity: {}, Mode: {}", identity_path, mode);

  if (send_cmd->parsed()) {
    command_handler.handle(radix_relay::events::send{ .peer = recipient, .message = send_message });
    return 0;
  }

  if (peers_cmd->parsed()) {
    command_handler.handle(radix_relay::events::peers{});
    return 0;
  }

  if (status_cmd->parsed()) {
    command_handler.handle(radix_relay::events::status{});
    return 0;
  }

  spdlog::info("Starting interactive mode");

  std::string node_fingerprint = std::string(radix_relay::get_node_identity_fingerprint());

  fmt::print("Radix Relay v{} - Interactive Mode\n", radix_relay::cmake::project_version);
  fmt::print("Node: {}\n", node_fingerprint);
  fmt::print("Transport: {} (internet + BLE mesh)\n", mode);
  fmt::print("Connected Peers: 0 (transport layer not implemented)\n\n");

  fmt::print(
    "Available commands: send, broadcast, peers, status, sessions, mode, scan, connect, trust, verify, version, help, "
    "quit\n\n");

  const radix_relay::StandardEventHandler event_handler{ command_handler };
  radix_relay::InteractiveCli cli(node_fingerprint, mode, event_handler);
  cli.run();

  return 0;
}
