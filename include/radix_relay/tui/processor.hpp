#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <radix_relay/signal/signal_bridge.hpp>
#include <string>
#include <vector>

namespace radix_relay::tui {

struct processor
{
  processor(std::string node_id, std::string mode, std::shared_ptr<radix_relay::signal::bridge> bridge)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(std::move(bridge))
  {}

  auto run() -> void;

  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

  auto add_message(const std::string &message) -> void;

private:
  template<typename EvtHandler>
  auto process_command(const std::string &input, std::shared_ptr<EvtHandler> evt_handler) -> void;

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<radix_relay::signal::bridge> bridge_;

  std::vector<std::string> messages_;
  std::string input_content_;
  ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();
};

}// namespace radix_relay::tui
