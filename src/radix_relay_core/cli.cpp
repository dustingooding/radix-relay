#include <radix_relay/cli.hpp>
#include <fmt/base.h>
#include <iostream>
#include <string>
#include <utility>

#include "internal_use_only/config.hpp"

namespace radix_relay {


InteractiveCli::InteractiveCli(std::string node_id, std::string mode)
    : node_id_(std::move(node_id)), mode_(std::move(mode)) {}

auto InteractiveCli::should_quit(const std::string& input) -> bool {
    if (input == "quit" || input == "exit" || input == "q") {
        fmt::print("Goodbye!\n");
        return true;
    }
    return false;
}

auto InteractiveCli::handle_command(const std::string& input) -> bool {
    if (input == "help") {
        handle_help();
        return true;
    }
    if (input == "peers") {
        handle_peers();
        return true;
    }
    if (input == "status") {
        handle_status();
        return true;
    }
    if (input == "sessions") {
        handle_sessions();
        return true;
    }
    if (input == "scan") {
        handle_scan();
        return true;
    }
    if (input == "version") {
        handle_version();
        return true;
    }
    
    constexpr auto mode_cmd = "mode ";
    if (input.starts_with(mode_cmd)) {
        handle_mode(input.substr(std::string_view(mode_cmd).length()));
        return true;
    }
    constexpr auto send_cmd = "send ";
    if (input.starts_with(send_cmd)) {
        handle_send(input.substr(std::string_view(send_cmd).length()));
        return true;
    }
    constexpr auto broadcast_cmd = "broadcast ";
    if (input.starts_with(broadcast_cmd)) {
        handle_broadcast(input.substr(std::string_view(broadcast_cmd).length()));
        return true;
    }
    constexpr auto connect_cmd = "connect ";
    if (input.starts_with(connect_cmd)) {
        handle_connect(input.substr(std::string_view(connect_cmd).length()));
        return true;
    }
    constexpr auto trust_cmd = "trust ";
    if (input.starts_with(trust_cmd)) {
        handle_trust(input.substr(std::string_view(trust_cmd).length()));
        return true;
    }
    constexpr auto verify_cmd = "verify ";
    if (input.starts_with(verify_cmd)) {
        handle_verify(input.substr(std::string_view(verify_cmd).length()));
        return true;
    }
    
    return false;
}

auto InteractiveCli::run() -> void {
    // NOLINTNEXTLINE(misc-const-correctness)
    std::string input;
    while (true) {
        fmt::print("{} [⇌] ", node_id_);
        
        if (!std::getline(std::cin, input)) {
            break; // EOF or Ctrl+D
        }
        
        if (input.empty()) {
            continue;
        }
        
        if (should_quit(input)) {
            break;
        }
        
        if (handle_command(input)) {
            continue;
        }
        
        // Unknown command
        fmt::print("Unknown command: '{}'. Type 'help' for available commands.\n", input);
    }
}

auto InteractiveCli::handle_help() -> void {
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

auto InteractiveCli::handle_peers() -> void {
    fmt::print("Connected Peers: (transport layer not implemented)\n");
    fmt::print("  No peers discovered yet\n");
}

auto InteractiveCli::handle_status() -> void {
    fmt::print("Network Status:\n");
    fmt::print("  ├─ Internet: Not connected (transport not implemented)\n");
    fmt::print("  ├─ BLE Mesh: Not initialized (transport not implemented)\n");
    fmt::print("  ├─ Active Sessions: 0\n");
    fmt::print("  └─ Messages: 0 sent, 0 received\n");
}

auto InteractiveCli::handle_sessions() -> void {
    fmt::print("Active Encrypted Sessions: (Signal Protocol not implemented)\n");
    fmt::print("  No active sessions\n");
}

auto InteractiveCli::handle_scan() -> void {
    fmt::print("Scanning for BLE peers... (BLE transport not implemented)\n");
    fmt::print("No peers found\n");
}

auto InteractiveCli::handle_mode(const std::string& new_mode) -> void {
    if (new_mode == "internet" || new_mode == "mesh" || new_mode == "hybrid") {
        mode_ = new_mode;
        fmt::print("Switched to {} mode\n", new_mode);
    } else {
        fmt::print("Invalid mode. Use: internet, mesh, or hybrid\n");
    }
}

auto InteractiveCli::handle_send(const std::string& args) -> void {
    auto first_space = args.find(' ');
    if (first_space != std::string::npos && !args.empty()) {
        const std::string peer = args.substr(0, first_space);
        std::string message = args.substr(first_space + 1);
        fmt::print("Sending '{}' to '{}' via {} transport (not implemented)\n", 
                  message, peer, mode_);
    } else {
        fmt::print("Usage: send <peer> <message>\n");
    }
}

auto InteractiveCli::handle_broadcast(const std::string& message) -> void {
    if (!message.empty()) {
        fmt::print("Broadcasting '{}' to all local peers (not implemented)\n", message);
    } else {
        fmt::print("Usage: broadcast <message>\n");
    }
}

auto InteractiveCli::handle_connect(const std::string& relay) -> void {
    if (!relay.empty()) {
        fmt::print("Connecting to Nostr relay {} (not implemented)\n", relay);
    } else {
        fmt::print("Usage: connect <relay>\n");
    }
}

auto InteractiveCli::handle_trust(const std::string& peer) -> void {
    if (!peer.empty()) {
        fmt::print("Marking {} as trusted (not implemented)\n", peer);
    } else {
        fmt::print("Usage: trust <peer>\n");
    }
}

auto InteractiveCli::handle_verify(const std::string& peer) -> void {
    if (!peer.empty()) {
        fmt::print("Safety numbers for {} (Signal Protocol not implemented)\n", peer);
    } else {
        fmt::print("Usage: verify <peer>\n");
    }
}

auto InteractiveCli::handle_version() -> void {
    fmt::print("Radix Relay v{}\n", radix_relay::cmake::project_version);
    fmt::print("Hybrid Mesh Communications System\n");
}

auto InteractiveCli::get_mode() const -> const std::string& {
    return mode_;
}

} // namespace radix_relay