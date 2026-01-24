#pragma once

#include <algorithm>
#include <core/events.hpp>
#include <string>
#include <vector>

namespace radix_relay_test {

/**
 * @brief Recording command handler that tracks which commands were dispatched.
 *
 * This is a test double that can be used as a CommandHandler template parameter
 * for event_handler in tests. It records all command invocations for verification.
 */
struct recording_command_handler
{
  mutable std::vector<std::string> called_commands;

  auto operator()(const radix_relay::core::events::help & /*command*/) const -> void
  {
    called_commands.push_back("help");
  }

  auto operator()(const radix_relay::core::events::peers & /*command*/) const -> void
  {
    called_commands.push_back("peers");
  }

  auto operator()(const radix_relay::core::events::status & /*command*/) const -> void
  {
    called_commands.push_back("status");
  }

  auto operator()(const radix_relay::core::events::sessions & /*command*/) const -> void
  {
    called_commands.push_back("sessions");
  }

  auto operator()(const radix_relay::core::events::scan & /*command*/) const -> void
  {
    called_commands.push_back("scan");
  }

  auto operator()(const radix_relay::core::events::version & /*command*/) const -> void
  {
    called_commands.push_back("version");
  }

  auto operator()(const radix_relay::core::events::mode &command) const -> void
  {
    called_commands.push_back("mode:" + command.new_mode);
  }

  auto operator()(const radix_relay::core::events::send &command) const -> void
  {
    called_commands.push_back("send:" + command.peer + ":" + command.message);
  }

  auto operator()(const radix_relay::core::events::broadcast &command) const -> void
  {
    called_commands.push_back("broadcast:" + command.message);
  }

  auto operator()(const radix_relay::core::events::connect &command) const -> void
  {
    called_commands.push_back("connect:" + command.relay);
  }

  auto operator()(const radix_relay::core::events::disconnect & /*command*/) const -> void
  {
    called_commands.push_back("disconnect");
  }

  auto operator()(const radix_relay::core::events::trust &command) const -> void
  {
    called_commands.push_back("trust:" + command.peer);
  }

  auto operator()(const radix_relay::core::events::verify &command) const -> void
  {
    called_commands.push_back("verify:" + command.peer);
  }

  auto operator()(const radix_relay::core::events::identities & /*command*/) const -> void
  {
    called_commands.push_back("identities");
  }

  auto operator()(const radix_relay::core::events::publish_identity & /*command*/) const -> void
  {
    called_commands.push_back("publish_identity");
  }

  auto operator()(const radix_relay::core::events::unpublish_identity & /*command*/) const -> void
  {
    called_commands.push_back("unpublish_identity");
  }

  auto operator()(const radix_relay::core::events::chat &command) const -> void
  {
    called_commands.push_back("chat:" + command.contact);
  }

  auto operator()(const radix_relay::core::events::leave & /*command*/) const -> void
  {
    called_commands.push_back("leave");
  }

  auto operator()(const radix_relay::core::events::unknown_command &command) const -> void
  {
    called_commands.push_back("unknown_command:" + command.input);
  }

  [[nodiscard]] auto was_called(const std::string &command) const -> bool
  {
    return std::any_of(called_commands.cbegin(), called_commands.cend(), [&command](const std::string &call) {
      return call.find(command) == 0;
    });
  }

  [[nodiscard]] auto get_call_count() const -> size_t { return called_commands.size(); }

  auto clear_calls() const -> void { called_commands.clear(); }
};

}// namespace radix_relay_test
