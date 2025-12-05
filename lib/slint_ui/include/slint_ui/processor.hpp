#pragma once

#include <async/async_queue.hpp>
#include <atomic>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <main_window.h>
#include <memory>
#include <slint.h>
#include <string>
#include <vector>

namespace radix_relay::slint_ui {

template<concepts::signal_bridge Bridge> struct processor
{
  processor(std::string node_id,
    std::string mode,
    const std::shared_ptr<Bridge> &bridge,
    const std::shared_ptr<async::async_queue<core::events::raw_command>> &command_queue,
    const std::shared_ptr<async::async_queue<core::events::display_message>> &display_queue);

  ~processor();

  processor(const processor &) = delete;
  auto operator=(const processor &) -> processor & = delete;
  processor(processor &&) = delete;
  auto operator=(processor &&) -> processor & = delete;

  auto run() -> void;

  auto stop() -> void;

  [[nodiscard]] auto get_mode() const -> const std::string & { return mode_; }

private:
  auto setup_timer() -> void;

  auto poll_display_messages() -> void;

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue_;
  std::shared_ptr<async::async_queue<core::events::display_message>> display_queue_;

  std::atomic<bool> running_{ false };

  std::shared_ptr<slint::ComponentHandle<MainWindow>> window_;
  std::shared_ptr<slint::VectorModel<Message>> message_model_;
  std::shared_ptr<slint::Timer> timer_;
};

}// namespace radix_relay::slint_ui
