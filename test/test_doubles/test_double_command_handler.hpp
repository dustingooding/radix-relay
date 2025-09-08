#ifndef TEST_DOUBLE_COMMAND_HANDLER_HPP
#define TEST_DOUBLE_COMMAND_HANDLER_HPP

#include <algorithm>
#include <radix_relay/concepts/command_handler.hpp>
#include <radix_relay/events/events.hpp>
#include <string>
#include <vector>

namespace radix_relay_test {

struct TestDoubleCommandHandler
{
  mutable std::vector<std::string> called_commands;

  template<radix_relay::events::Command T> auto handle(const T &command) const -> void { handle_impl(command); }

private:
  auto handle_impl(const radix_relay::events::help & /*command*/) const -> void { called_commands.push_back("help"); }

  auto handle_impl(const radix_relay::events::peers & /*command*/) const -> void { called_commands.push_back("peers"); }

  auto handle_impl(const radix_relay::events::status & /*command*/) const -> void
  {
    called_commands.push_back("status");
  }

  auto handle_impl(const radix_relay::events::sessions & /*command*/) const -> void
  {
    called_commands.push_back("sessions");
  }

  auto handle_impl(const radix_relay::events::scan & /*command*/) const -> void { called_commands.push_back("scan"); }

  auto handle_impl(const radix_relay::events::version & /*command*/) const -> void
  {
    called_commands.push_back("version");
  }

  auto handle_impl(const radix_relay::events::mode &command) const -> void
  {
    called_commands.push_back("mode:" + command.new_mode);
  }

  auto handle_impl(const radix_relay::events::send &command) const -> void
  {
    called_commands.push_back("send:" + command.peer + ":" + command.message);
  }

  auto handle_impl(const radix_relay::events::broadcast &command) const -> void
  {
    called_commands.push_back("broadcast:" + command.message);
  }

  auto handle_impl(const radix_relay::events::connect &command) const -> void
  {
    called_commands.push_back("connect:" + command.relay);
  }

  auto handle_impl(const radix_relay::events::trust &command) const -> void
  {
    called_commands.push_back("trust:" + command.peer);
  }

  auto handle_impl(const radix_relay::events::verify &command) const -> void
  {
    called_commands.push_back("verify:" + command.peer);
  }

public:
  auto was_called(const std::string &command) const -> bool
  {
    return std::any_of(called_commands.cbegin(), called_commands.cend(), [&command](const std::string &call) {
      return call.find(command) == 0;
    });
  }

  auto get_call_count() const -> size_t { return called_commands.size(); }

  auto clear_calls() const -> void { called_commands.clear(); }
};

static_assert(radix_relay::concepts::CommandHandler<TestDoubleCommandHandler>);

}// namespace radix_relay_test

#endif
