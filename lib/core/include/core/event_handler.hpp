#pragma once

#include <concepts/command_handler.hpp>
#include <core/events.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace radix_relay::core {

template<concepts::command_handler CmdHandler> struct event_handler
{
  using command_handler_t = CmdHandler;

  explicit event_handler(std::shared_ptr<CmdHandler> command_handler) : command_handler_(command_handler) {}

  auto handle(const events::raw_command &event) const -> void
  {
    const auto &input = event.input;

    if (input == "help") {
      command_handler_->handle(events::help{});
      return;
    }
    if (input == "peers") {
      command_handler_->handle(events::peers{});
      return;
    }
    if (input == "status") {
      command_handler_->handle(events::status{});
      return;
    }
    if (input == "sessions") {
      command_handler_->handle(events::sessions{});
      return;
    }
    if (input == "scan") {
      command_handler_->handle(events::scan{});
      return;
    }
    if (input == "version") {
      command_handler_->handle(events::version{});
      return;
    }

    constexpr auto mode_cmd = "mode ";
    if (input.starts_with(mode_cmd)) {
      command_handler_->handle(events::mode{ .new_mode = input.substr(std::string_view(mode_cmd).length()) });
      return;
    }

    constexpr auto send_cmd = "send ";
    if (input.starts_with(send_cmd)) {
      const auto args = input.substr(std::string_view(send_cmd).length());
      const auto first_space = args.find(' ');
      if (first_space != std::string::npos and not args.empty()) {
        command_handler_->handle(
          events::send{ .peer = args.substr(0, first_space), .message = args.substr(first_space + 1) });
      } else {
        command_handler_->handle(events::send{ .peer = "", .message = "" });
      }
      return;
    }

    constexpr auto broadcast_cmd = "broadcast ";
    if (input.starts_with(broadcast_cmd)) {
      command_handler_->handle(events::broadcast{ .message = input.substr(std::string_view(broadcast_cmd).length()) });
      return;
    }

    constexpr auto connect_cmd = "connect ";
    if (input.starts_with(connect_cmd)) {
      command_handler_->handle(events::connect{ .relay = input.substr(std::string_view(connect_cmd).length()) });
      return;
    }

    if (input == "disconnect") {
      command_handler_->handle(events::disconnect{});
      return;
    }

    constexpr auto trust_cmd = "trust ";
    if (input.starts_with(trust_cmd)) {
      command_handler_->handle(
        events::trust{ .peer = input.substr(std::string_view(trust_cmd).length()), .alias = "" });
      return;
    }

    constexpr auto verify_cmd = "verify ";
    if (input.starts_with(verify_cmd)) {
      command_handler_->handle(events::verify{ .peer = input.substr(std::string_view(verify_cmd).length()) });
      return;
    }
  }

private:
  std::shared_ptr<CmdHandler> command_handler_;
};

}// namespace radix_relay::core
