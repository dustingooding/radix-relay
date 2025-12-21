#pragma once

#include <algorithm>
#include <async/async_queue.hpp>
#include <atomic>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <memory>
#include <replxx.hxx>
#include <string>
#include <thread>

namespace radix_relay::tui {

/**
 * @brief Text-based user interface processor with REPL.
 *
 * @tparam Bridge Type satisfying the signal_bridge concept
 *
 * Provides an interactive command-line interface using Replxx for command input,
 * history management, and display message output.
 */
template<concepts::signal_bridge Bridge> struct processor
{
  /**
   * @brief Constructs a TUI processor.
   *
   * @param node_id Node identifier for display
   * @param mode Network mode (internet/mesh/hybrid)
   * @param bridge Signal Protocol bridge
   * @param command_queue Queue for outgoing user commands
   * @param display_queue Queue for incoming display messages
   */
  processor(std::string node_id,
    std::string mode,
    const std::shared_ptr<Bridge> &bridge,
    const std::shared_ptr<async::async_queue<core::events::raw_command>> &command_queue,
    const std::shared_ptr<async::async_queue<core::events::display_message>> &display_queue)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge), command_queue_(command_queue),
      display_queue_(display_queue)
  {}

  ~processor() { stop(); }

  processor(const processor &) = delete;
  auto operator=(const processor &) -> processor & = delete;
  processor(processor &&) = delete;
  auto operator=(processor &&) -> processor & = delete;

  /**
   * @brief Runs the interactive REPL loop.
   *
   * Starts message polling thread and processes user input until quit command.
   */
  auto run() -> void
  {
    setup_replxx();

    print_message("Radix Relay - Interactive Mode");
    print_message("Node: " + node_id_);
    print_message("Mode: " + mode_);
    print_message("");
    print_message("Type 'help' for available commands, 'quit' to exit");
    print_message("");

    running_.store(true);

    message_poller_ = std::thread([this] {
      while (running_.load()) {
        while (auto msg = display_queue_->try_pop()) { print_message(msg->message); }
        std::this_thread::sleep_for(std::chrono::milliseconds(message_poll_interval_ms));
      }
    });

    while (running_.load()) {
      const char *input = rx_.input(prompt_);

      if (input == nullptr) {
        running_.store(false);
        break;
      }

      std::string command(input);

      command.erase(0, command.find_first_not_of(" \t\r\n"));
      command.erase(command.find_last_not_of(" \t\r\n") + 1);

      if (command.empty()) { continue; }

      if (command == "quit" or command == "exit" or command == "q") {
        print_message("Goodbye!");
        running_.store(false);
        break;
      }

      rx_.history_add(command);
      process_command(command);
    }

    stop();
    save_history();
  }

  /**
   * @brief Stops the TUI processor and message polling thread.
   */
  auto stop() -> void
  {
    if (running_.exchange(false)) {
      if (message_poller_.joinable()) { message_poller_.join(); }
    }
  }

  /**
   * @brief Returns the current network mode.
   *
   * @return Network mode string (internet/mesh/hybrid)
   */
  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

private:
  /**
   * @brief Configures Replxx with history and command completion.
   */
  auto setup_replxx() -> void
  {
    prompt_ = std::string(GREEN) + "[⇌] " + RESET;

    rx_.history_load(HISTORY_FILE);

    rx_.set_max_history_size(MAX_HISTORY_SIZE);
    rx_.set_max_hint_rows(MAX_HINT_ROWS);

    rx_.set_completion_callback([](const std::string &input, int & /*context*/) {
      replxx::Replxx::completions_t completions;

      std::vector<std::string> commands = {
        "connect", "send", "trust", "list", "publish", "unpublish", "subscribe", "help", "mode", "quit", "exit"
      };

      std::copy_if(commands.begin(), commands.end(), std::back_inserter(completions), [&input](const std::string &cmd) {
        return cmd.starts_with(input);
      });

      return completions;
    });
  }

  /**
   * @brief Displays a message in the TUI.
   *
   * @param message Message text to display
   */
  auto print_message(const std::string &message) -> void
  {
    auto msg = message;
    if (not msg.empty() and msg.back() == '\n') { msg.pop_back(); }
    auto formatted = std::string(GREEN) + msg + RESET + "\n";
    rx_.write(formatted.c_str(), static_cast<int>(formatted.size()));
  }

  /**
   * @brief Processes user input and dispatches commands.
   *
   * @param input User command string
   */
  auto process_command(const std::string &input) -> void
  {
    constexpr auto mode_cmd = "/mode ";
    if (input.starts_with(mode_cmd)) {
      const auto new_mode = input.substr(std::string_view(mode_cmd).length());
      if (new_mode == "internet" or new_mode == "mesh" or new_mode == "hybrid") {
        mode_ = std::string(new_mode);
        print_message("Switched to " + std::string(new_mode) + " mode");
      } else {
        print_message("Invalid mode. Use: internet, mesh, or hybrid");
      }
      return;
    }

    command_queue_->push(core::events::raw_command{ .input = std::string(input) });
  }

  /**
   * @brief Saves command history to disk.
   */
  auto save_history() -> void { rx_.history_save(HISTORY_FILE); }

  static constexpr const char *GREEN = "\033[32m";
  static constexpr const char *RESET = "\033[0m";
  static constexpr const char *HISTORY_FILE = ".radix_relay_history";
  static constexpr auto message_poll_interval_ms = 16;
  static constexpr auto MAX_HISTORY_SIZE = 1000;
  static constexpr auto MAX_HINT_ROWS = 3;

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue_;
  std::shared_ptr<async::async_queue<core::events::display_message>> display_queue_;

  std::atomic<bool> running_{ false };
  std::thread message_poller_;
  replxx::Replxx rx_;
  std::string prompt_;
};

}// namespace radix_relay::tui
