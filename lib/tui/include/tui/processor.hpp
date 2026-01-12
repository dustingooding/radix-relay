#pragma once

#include <algorithm>
#include <async/async_queue.hpp>
#include <atomic>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <core/overload.hpp>
#include <memory>
#include <optional>
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
    const std::shared_ptr<async::async_queue<core::events::ui_event_t>> &ui_event_queue)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge), command_queue_(command_queue),
      ui_event_queue_(ui_event_queue), prompt_(std::string(GREEN) + "[⇌] " + RESET)
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
    print_message("Type '/help' for available commands, '/quit' to exit");
    print_message("");

    running_.store(true);

    while (running_.load()) {
      // Process any pending UI events before showing the prompt
      while (auto event = ui_event_queue_->try_pop()) { process_ui_event(*event); }

      const char *input = rx_.input(prompt_);

      if (input == nullptr) {
        running_.store(false);
        break;
      }

      std::string command(input);

      command.erase(0, command.find_first_not_of(" \t\r\n"));
      command.erase(command.find_last_not_of(" \t\r\n") + 1);

      if (command.empty()) { continue; }

      if (command == "/quit" or command == "/exit" or command == "/q") {
        print_message("Goodbye!");
        running_.store(false);
        break;
      }

      rx_.history_add(command);
      process_command(command);

      // Poll for UI events with timeout to allow pipeline to process
      constexpr auto max_wait_ms = 100;
      constexpr auto poll_interval_ms = 5;
      for (int waited = 0; waited < max_wait_ms; waited += poll_interval_ms) {
        if (auto event = ui_event_queue_->try_pop()) {
          // Found an event, process it and all other pending events
          process_ui_event(*event);
          while (auto next_event = ui_event_queue_->try_pop()) { process_ui_event(*next_event); }
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
      }
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

  /**
   * @brief Updates the active chat context with a contact name.
   *
   * @param contact_name Display name or alias of the contact
   */
  auto update_chat_context(std::string contact_name) -> void
  {
    active_chat_context_ = std::move(contact_name);
    update_prompt();
  }

  /**
   * @brief Clears the active chat context, returning to default mode.
   */
  auto clear_chat_context() -> void
  {
    active_chat_context_.reset();
    update_prompt();
  }

  /**
   * @brief Returns the current chat context.
   *
   * @return Contact name if in chat mode, nullopt otherwise
   */
  [[nodiscard]] auto get_chat_context() const -> std::optional<std::string> { return active_chat_context_; }

  /**
   * @brief Returns the current prompt string.
   *
   * @return Prompt string with chat indicator if in chat mode
   */
  [[nodiscard]] auto get_prompt() const -> const std::string & { return prompt_; }

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

      std::vector<std::string> commands = { "/connect",
        "/send",
        "/trust",
        "/list",
        "/publish",
        "/unpublish",
        "/subscribe",
        "/help",
        "/mode",
        "/quit",
        "/exit",
        "/status" };

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

  /**
   * @brief Updates the Replxx prompt based on current chat context.
   */
  auto update_prompt() -> void
  {
    if (active_chat_context_.has_value()) {
      prompt_ = std::string(GREEN) + "[⇌ @" + active_chat_context_.value() + "] " + RESET;
    } else {
      prompt_ = std::string(GREEN) + "[⇌] " + RESET;
    }
  }

  /**
   * @brief Processes UI events using variant visitor pattern.
   *
   * @param event UI event to process (display_message, enter_chat_mode, or exit_chat_mode)
   */
  auto process_ui_event(const core::events::ui_event_t &event) -> void
  {
    std::visit(core::overload{ [this](const core::events::display_message &evt) { print_message(evt.message); },
                 [this](const core::events::enter_chat_mode &evt) { update_chat_context(evt.display_name); },
                 [this](const core::events::exit_chat_mode &) { clear_chat_context(); } },
      event);
  }

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
  std::shared_ptr<async::async_queue<core::events::ui_event_t>> ui_event_queue_;

  std::atomic<bool> running_{ false };
  std::thread message_poller_;
  replxx::Replxx rx_;
  std::string prompt_;
  std::optional<std::string> active_chat_context_;
};

}// namespace radix_relay::tui
