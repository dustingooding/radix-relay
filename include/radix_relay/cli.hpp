#ifndef RADIX_RELAY_CLI_HPP
#define RADIX_RELAY_CLI_HPP

#include <radix_relay/core_export.hpp>
#include <string>

namespace radix_relay {

class RADIX_RELAY_CORE_EXPORT InteractiveCli {
public:
    InteractiveCli(std::string node_id, std::string mode);
    auto run() -> void;
    
    static auto should_quit(const std::string& input) -> bool;
    auto handle_command(const std::string& input) -> bool;
    
    static auto handle_help() -> void;
    static auto handle_peers() -> void;
    static auto handle_status() -> void;
    static auto handle_sessions() -> void;
    static auto handle_scan() -> void;
    static auto handle_version() -> void;
    auto handle_mode(const std::string& new_mode) -> void;
    auto handle_send(const std::string& args) -> void;
    static auto handle_broadcast(const std::string& message) -> void;
    static auto handle_connect(const std::string& relay) -> void;
    static auto handle_trust(const std::string& peer) -> void;
    static auto handle_verify(const std::string& peer) -> void;
    
    auto get_mode() const -> const std::string&;
    
private:
    std::string node_id_;
    std::string mode_;
};

} // namespace radix_relay

#endif
