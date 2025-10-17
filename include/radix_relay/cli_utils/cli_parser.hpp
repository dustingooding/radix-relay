#pragma once

#include <CLI/CLI.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace radix_relay {

struct cli_args
{
  std::string identity_path = "~/.radix/identity.db";
  std::string mode = "hybrid";
  bool verbose = false;
  bool show_version = false;

  bool send_parsed = false;
  std::string send_recipient;
  std::string send_message;

  bool peers_parsed = false;
  bool status_parsed = false;
};

inline auto setup_cli_app(CLI::App &app, cli_args &args) -> void;

inline auto parse_cli_args(int argc, char **argv) -> cli_args
{
  cli_args args;
  CLI::App app{ "Radix Relay - Hybrid Mesh Communications", "radix-relay" };

  setup_cli_app(app, args);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    app.exit(e);
    std::exit(e.get_exit_code());// NOLINT(concurrency-mt-unsafe)
  }

  args.identity_path = platform::expand_tilde_path(args.identity_path);

  return args;
}

inline auto setup_cli_app(CLI::App &app, cli_args &args) -> void
{
  app.add_option("-i,--identity", args.identity_path, "Path to identity key file");
  app.add_option("-m,--mode", args.mode, "transport mode: internet, mesh, hybrid")
    ->check(CLI::IsMember({ "internet", "mesh", "hybrid" }));
  app.add_flag("-v,--verbose", args.verbose, "Enable verbose logging");
  app.add_flag("--version", args.show_version, "Show version information");

  auto *send_cmd = app.add_subcommand("send", "Send a message");
  send_cmd->add_option("recipient", args.send_recipient, "Node ID or contact name")->required();
  send_cmd->add_option("message", args.send_message, "Message content")->required();
  send_cmd->callback([&args]() { args.send_parsed = true; });

  auto *peers_cmd = app.add_subcommand("peers", "List discovered peers");
  peers_cmd->callback([&args]() { args.peers_parsed = true; });

  auto *status_cmd = app.add_subcommand("status", "Show network status");
  status_cmd->callback([&args]() { args.status_parsed = true; });
}

inline auto validate_cli_args(const cli_args &args) -> bool
{
  if (args.mode != "internet" and args.mode != "mesh" and args.mode != "hybrid") {
    spdlog::error("Invalid mode: {}", args.mode);
    return false;
  }

  if (args.send_parsed) {
    if (args.send_recipient.empty()) {
      spdlog::error("Send command requires recipient");
      return false;
    }
    if (args.send_message.empty()) {
      spdlog::error("Send command requires message");
      return false;
    }
  }

  return true;
}

}// namespace radix_relay
