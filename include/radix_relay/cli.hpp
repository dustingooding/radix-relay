#pragma once

#include <fmt/core.h>
#include <iostream>
#include <memory>
#include <radix_relay/concepts/event_handler.hpp>
#include <radix_relay/events/events.hpp>
#include <string>
#include <string_view>

namespace radix_relay {

template<concepts::event_handler EvtHandler> struct interactive_cli
{
  interactive_cli(std::string node_id, std::string mode, std::shared_ptr<EvtHandler> event_handler)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), event_handler_(std::move(event_handler))
  {}

  auto run() -> void
  {
    std::string input;
    while (true) {
      fmt::print("[⇌] ");

      if (!std::getline(std::cin, input)) {
        break;// EOF or Ctrl+D
      }

      if (input.empty()) { continue; }

      if (should_quit(input)) { break; }

      if (handle_command(input)) { continue; }

      fmt::print("Unknown command: '{}'. Type 'help' for available commands.\n", input);
    }
  }

  static auto should_quit(const std::string &input) -> bool
  {
    if (input == "quit" || input == "exit" || input == "q") {
      fmt::print("Goodbye!\n");
      return true;
    }
    return false;
  }

  auto handle_command(const std::string &input) -> bool
  {
    constexpr auto mode_cmd = "mode ";
    if (input.starts_with(mode_cmd)) {
      const auto new_mode = input.substr(std::string_view(mode_cmd).length());
      if (new_mode == "internet" || new_mode == "mesh" || new_mode == "hybrid") {
        mode_ = new_mode;
        fmt::print("Switched to {} mode\n", new_mode);
      } else {
        fmt::print("Invalid mode. Use: internet, mesh, or hybrid\n");
      }
      return true;
    }

    event_handler_->handle(events::raw_command{ .input = input });
    return true;
  }

  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

private:
  std::string node_id_;
  std::string mode_;
  std::shared_ptr<EvtHandler> event_handler_;
};

}// namespace radix_relay
