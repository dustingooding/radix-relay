#include <string>
#include <cstddef>

#include <CLI/CLI.hpp>
#include <fmt/base.h>
#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <radix_relay/cli.hpp>

#include "internal_use_only/config.hpp"

namespace {
    constexpr std::size_t NODE_ID_SUFFIX_LENGTH = 4;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
    CLI::App app{"Radix Relay - Hybrid Mesh Communications", "radix-relay"};

    std::string identity_path = "~/.radix/identity.key";
    std::string mode = "hybrid";
    bool verbose = false;
    bool show_version = false;
    
    app.add_option("-i,--identity", identity_path, "Path to identity key file");
    app.add_option("-m,--mode", mode, "Transport mode: internet, mesh, hybrid")->check(CLI::IsMember({"internet", "mesh", "hybrid"}));
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
    app.add_flag("--version", show_version, "Show version information");

    // Subcommands
    auto *send_cmd = app.add_subcommand("send", "Send a message");
    std::string recipient;
    std::string send_message;
    send_cmd->add_option("recipient", recipient, "Node ID or contact name")->required();
    send_cmd->add_option("message", send_message, "Message content")->required();

    auto *peers_cmd = app.add_subcommand("peers", "List discovered peers");
    
    auto *status_cmd = app.add_subcommand("status", "Show network status");

    CLI11_PARSE(app, argc, argv);

    // Handle version flag
    if (show_version) {
        fmt::print("Radix Relay v{}\n", radix_relay::cmake::project_version);
        return 0;
    }

    // Configure logging
    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    }

    spdlog::info("Radix Relay {} starting", radix_relay::cmake::project_version);
    spdlog::info("Identity: {}, Mode: {}", identity_path, mode);

    if (send_cmd->parsed()) {
        spdlog::info("Sending message '{}' to '{}'", send_message, recipient);
        fmt::print("Message queued for delivery via {} transport(s)\n", mode);
        return 0;
    }
    
    if (peers_cmd->parsed()) {
        fmt::print("Discovered peers: (none - transport layer not implemented)\n");
        return 0;
    }
    
    if (status_cmd->parsed()) {
        fmt::print("Network Status:\n");
        fmt::print("  Internet: Not connected\n");
        fmt::print("  BLE Mesh: Not initialized\n");
        fmt::print("  Active Sessions: 0\n");
        return 0;
    }

    // Interactive mode
    spdlog::info("Starting interactive mode");
    
    // Generate a short node ID from identity path for the prompt
    std::string node_id = "node-" + identity_path.substr(identity_path.find_last_of('/') + 1, NODE_ID_SUFFIX_LENGTH);
    
    fmt::print("Radix Relay v{} - Interactive Mode\n", radix_relay::cmake::project_version);
    fmt::print("Identity: {}\n", node_id);
    fmt::print("Transport: {} (internet + BLE mesh)\n", mode);
    fmt::print("Connected Peers: 0 (transport layer not implemented)\n\n");
    
    fmt::print("Available commands: send, broadcast, peers, status, sessions, mode, scan, connect, trust, verify, version, help, quit\n\n");
    
    radix_relay::InteractiveCli cli(node_id, mode);
    cli.run();
    
    return 0;
}