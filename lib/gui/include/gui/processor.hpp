#pragma once

#include <async/async_queue.hpp>
#include <atomic>
#include <chrono>
#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <fmt/format.h>
#include <main_window.h>
#include <memory>
#include <platform/time_utils.hpp>
#include <slint.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace radix_relay::gui {

template<concepts::signal_bridge Bridge> struct processor
{
  processor(std::string node_id,
    std::string mode,
    const std::shared_ptr<Bridge> &bridge,
    const std::shared_ptr<async::async_queue<core::events::raw_command>> &command_queue,
    const std::shared_ptr<async::async_queue<core::events::display_message>> &display_queue,
    const slint::ComponentHandle<MainWindow> &window,
    const std::shared_ptr<slint::VectorModel<Message>> &message_model)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge), command_queue_(command_queue),
      display_queue_(display_queue), window_(window), message_model_(message_model)
  {
    window_->set_node_fingerprint(slint::SharedString(node_id_));
    window_->set_current_mode(slint::SharedString(mode_));
    window_->set_messages(message_model_);

    window_->on_send_command([this](const slint::SharedString &command) {
      std::string cmd(command.data(), command.size());
      if (cmd.empty()) { return; }

      if (cmd == "/quit" or cmd == "/exit" or cmd == "/q") {
        running_.store(false);
        window_->hide();
        return;
      }

      command_queue_->push(core::events::raw_command{ .input = std::move(cmd) });
    });
  }

  ~processor() { stop(); }

  processor(const processor &) = delete;
  auto operator=(const processor &) -> processor & = delete;
  processor(processor &&) = delete;
  auto operator=(processor &&) -> processor & = delete;

  auto run() -> void
  {
    running_.store(true);

    setup_timer();

    window_->run();

    stop();
  }

  auto stop() -> void
  {
    if (running_.exchange(false)) {
      if (timer_) { timer_.reset(); }
      spdlog::debug("GUI processor stopped");
    }
  }

  auto poll_display_messages() -> void
  {
    constexpr std::size_t max_messages_per_poll = 10;
    std::size_t processed = 0;

    while (processed < max_messages_per_poll) {
      auto msg = display_queue_->try_pop();
      if (not msg.has_value()) { break; }

      auto content = msg->message;
      content.erase(content.find_last_not_of("\r\n") + 1);

      Message ui_msg;
      ui_msg.content = slint::SharedString(content);
      ui_msg.timestamp = slint::SharedString(platform::format_current_time_hms());
      message_model_->push_back(ui_msg);
      ++processed;
    }
  }

  [[nodiscard]] auto get_message_model() const -> std::shared_ptr<slint::VectorModel<Message>>
  {
    return message_model_;
  }

  [[nodiscard]] auto is_running() const -> bool { return running_.load(); }

private:
  auto setup_timer() -> void
  {
    constexpr auto poll_interval_ms = 16;
    timer_ = std::make_shared<slint::Timer>();
    timer_->start(slint::TimerMode::Repeated, std::chrono::milliseconds(poll_interval_ms), [this]() {
      if (not running_.load()) { return; }
      poll_display_messages();
    });
  }

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue_;
  std::shared_ptr<async::async_queue<core::events::display_message>> display_queue_;

  std::atomic<bool> running_{ false };

  slint::ComponentHandle<MainWindow> window_;
  std::shared_ptr<slint::VectorModel<Message>> message_model_;
  std::shared_ptr<slint::Timer> timer_;
};

[[nodiscard]] inline auto make_window() -> slint::ComponentHandle<MainWindow> { return MainWindow::create(); }

[[nodiscard]] inline auto make_message_model() -> std::shared_ptr<slint::VectorModel<Message>>
{
  return std::make_shared<slint::VectorModel<Message>>();
}

}// namespace radix_relay::gui
