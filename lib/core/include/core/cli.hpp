#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <fmt/core.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace radix_relay::core {

/**
 * @brief Interactive command-line interface for user input.
 *
 * Provides a blocking REPL that reads user commands from stdin and
 * queues them for processing.
 */
struct interactive_cli
{
  /**
   * @brief Constructs an interactive CLI.
   *
   * @param node_id Node identifier for display
   * @param mode Initial transport mode
   * @param command_queue Queue for outgoing raw commands
   */
  interactive_cli(std::string node_id,
    std::string mode,
    std::shared_ptr<async::async_queue<events::raw_command>> command_queue)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), command_queue_(std::move(command_queue))
  {}

  /**
   * @brief Runs the interactive CLI loop (blocking).
   *
   * Reads lines from stdin and processes commands until user quits or EOF.
   */
  auto run() -> void
  {
    std::string input;
    while (true) {
      fmt::print("[⇌] ");

      if (not std::getline(std::cin, input)) {
        break;// EOF or Ctrl+D
      }

      if (input.empty()) { continue; }

      if (should_quit(input)) { break; }

      if (handle_command(input)) { continue; }

      fmt::print("Unknown command: '{}'. Type 'help' for available commands.\n", input);
    }
  }

  /**
   * @brief Checks if the input is a quit command.
   *
   * @param input User input string
   * @return true if user wants to quit, false otherwise
   */
  [[nodiscard]] static auto should_quit(const std::string &input) -> bool
  {
    if (input == "quit" or input == "exit" or input == "q") {
      fmt::print("Goodbye!\n");
      return true;
    }
    return false;
  }

  /**
   * @brief Handles a user command.
   *
   * @param input User input string
   * @return true if command was handled, false otherwise
   */
  [[nodiscard]] auto handle_command(const std::string &input) -> bool
  {
    constexpr auto mode_cmd = "/mode ";
    if (input.starts_with(mode_cmd)) {
      const auto new_mode = input.substr(std::string_view(mode_cmd).length());
      if (new_mode == "internet" or new_mode == "mesh" or new_mode == "hybrid") {
        mode_ = new_mode;
        fmt::print("Switched to {} mode\n", new_mode);
      } else {
        fmt::print("Invalid mode. Use: internet, mesh, or hybrid\n");
      }
      return true;
    }

    command_queue_->push(events::raw_command{ .input = input });
    return true;
  }

  /**
   * @brief Returns the current transport mode.
   *
   * @return Current mode string
   */
  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

private:
  std::string node_id_;
  std::string mode_;
  std::shared_ptr<async::async_queue<events::raw_command>> command_queue_;
};

}// namespace radix_relay::core
