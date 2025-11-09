#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/tui/scroll_state.hpp>
#include <string>
#include <thread>
#include <vector>

namespace radix_relay::tui {

template<concepts::signal_bridge Bridge> struct processor
{
  processor(std::string node_id,
    std::string mode,
    std::shared_ptr<Bridge> bridge,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue,// NOLINT(modernize-pass-by-value)
    std::shared_ptr<async::async_queue<core::events::display_message>> output_queue)// NOLINT(modernize-pass-by-value)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge),
      command_queue_(command_queue),// NOLINT(performance-unnecessary-value-param)
      output_queue_(output_queue)// NOLINT(performance-unnecessary-value-param)
  {}

  ~processor() { stop(); }

  processor(const processor &) = delete;
  auto operator=(const processor &) -> processor & = delete;
  processor(processor &&) = delete;
  auto operator=(processor &&) -> processor & = delete;

  auto run() -> void
  {
    using namespace ftxui;

    add_message("Radix Relay - Interactive Mode");
    add_message("Node: " + node_id_);
    add_message("Mode: " + mode_);
    add_message("");
    add_message("Type 'help' for available commands, 'quit' to exit");
    add_message("");

    InputOption input_option;
    input_option.on_enter = [&] {
      process_command(input_content_);
      input_content_.clear();
    };
    input_option.placeholder = "Type command...";

    input_option.transform = [](InputState state) {
      state.element |= color(Color::Green);
      if (state.is_placeholder) { state.element |= dim; }
      return state.element;
    };

    auto input_box = Input(&input_content_, input_option);

    scroll_state scroll;
    float scroll_y = 1.0F;
    constexpr float scroll_increment = 0.05F;
    constexpr float scroll_bottom_threshold = 0.99F;

    auto messages_content = Renderer([&] {
      if (messages_.empty()) { return text("No messages yet") | color(Color::Green) | center | flex; }

      const Elements messages_elements = [&] {
        Elements elements;
        elements.reserve(messages_.size());
        std::ranges::transform(messages_, std::back_inserter(elements), [](const std::string &msg) {
          return text(msg) | color(Color::Green);
        });
        return elements;
      }();

      return vbox(messages_elements);
    });

    auto messages_component = Renderer(messages_content, [&, messages_content] {
      auto content = messages_content->Render();

      if (scroll.should_stick_to_bottom()) { scroll_y = 1.0F; }

      content = content | focusPositionRelative(0.0F, scroll_y) | frame | vscroll_indicator | flex;
      return content;
    });

    messages_component = CatchEvent(messages_component, [&](Event event) {
      if (event.is_mouse() and event.mouse().button == Mouse::WheelUp) {
        scroll.handle_wheel_up();
        scroll_y = std::max(0.0F, scroll_y - scroll_increment);
        return true;
      }
      if (event.is_mouse() and event.mouse().button == Mouse::WheelDown) {
        scroll_y = std::min(1.0F, scroll_y + scroll_increment);
        if (scroll_y >= scroll_bottom_threshold) { scroll.reset_to_bottom(); }
        return true;
      }
      if (event == Event::End) {
        scroll.handle_end_key();
        scroll_y = 1.0F;
        return true;
      }
      return false;
    });

    auto main_container = Container::Vertical({
      messages_component,
      input_box,
    });

    auto main_renderer = Renderer(main_container, [&] {
      return vbox({
               messages_component->Render() | flex,
               separator() | color(Color::Green),
               hbox({
                 text("[⇌] ") | color(Color::Green),
                 input_box->Render() | flex,
               }),
             })
             | bgcolor(Color::Black);
    });

    input_box->TakeFocus();

    running_.store(true);
    message_poller_ = std::thread([this] {
      while (running_.load()) {
        bool had_messages = false;
        while (auto msg = output_queue_->try_pop()) {
          add_message(msg->message);
          had_messages = true;
        }
        if (had_messages) { screen_.PostEvent(ftxui::Event::Custom); }
        std::this_thread::sleep_for(std::chrono::milliseconds(message_poll_interval_ms));
      }
    });

    screen_.Loop(main_renderer);

    stop();
  }

  auto stop() -> void
  {
    if (running_.exchange(false)) {
      if (message_poller_.joinable()) { message_poller_.join(); }
    }
  }

  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

  auto add_message(const std::string &message) -> void { messages_.emplace_back(message); }

private:
  auto process_command(const std::string &input) -> void
  {
    auto trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty()) { return; }

    if (trimmed == "quit" or trimmed == "exit" or trimmed == "q") {
      add_message("[⇌] " + trimmed);
      add_message("Goodbye!");
      screen_.Exit();
      return;
    }

    add_message("[⇌] " + trimmed);

    constexpr auto mode_cmd = "mode ";
    if (trimmed.starts_with(mode_cmd)) {
      const auto new_mode = trimmed.substr(std::string_view(mode_cmd).length());
      if (new_mode == "internet" or new_mode == "mesh" or new_mode == "hybrid") {
        mode_ = std::string(new_mode);
        add_message("Switched to " + std::string(new_mode) + " mode");
      } else {
        add_message("Invalid mode. Use: internet, mesh, or hybrid");
      }
      return;
    }

    command_queue_->push(core::events::raw_command{ .input = std::string(trimmed) });
  }

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue_;
  std::shared_ptr<async::async_queue<core::events::display_message>> output_queue_;

  std::vector<std::string> messages_;
  std::string input_content_;
  ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();

  std::atomic<bool> running_{ false };
  std::thread message_poller_;
  static constexpr auto message_poll_interval_ms = 16;
};

}// namespace radix_relay::tui
