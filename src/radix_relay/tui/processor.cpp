#include <radix_relay/tui/processor.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <radix_relay/core/command_handler.hpp>
#include <radix_relay/core/event_handler.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/tui/printer.hpp>
#include <radix_relay/tui/scroll_state.hpp>

namespace radix_relay::tui {

using namespace ftxui;

auto processor::run() -> void
{
  auto tui_printer = std::make_shared<printer>([this](const std::string &message) { add_message(message); });
  auto cmd_handler = std::make_shared<core::command_handler<signal::bridge, printer>>(bridge_, tui_printer);
  auto evt_handler = std::make_shared<core::event_handler<core::command_handler<signal::bridge, printer>>>(cmd_handler);

  add_message("Radix Relay - Interactive Mode");
  add_message("Node: " + node_id_);
  add_message("Mode: " + mode_);
  add_message("");
  add_message("Type 'help' for available commands, 'quit' to exit");
  add_message("");

  InputOption input_option;
  input_option.on_enter = [&, evt_handler] {
    process_command(input_content_, evt_handler);
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

  screen_.Loop(main_renderer);
}

template<typename EvtHandler>
auto processor::process_command(const std::string &input, std::shared_ptr<EvtHandler> evt_handler) -> void
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

  evt_handler->handle(core::events::raw_command{ .input = std::string(trimmed) });
}

auto processor::add_message(const std::string &message) -> void { messages_.emplace_back(message); }

}// namespace radix_relay::tui
