#include <slint_ui/processor.hpp>

#include <chrono>
#include <iomanip>
#include <main_window.h>
#include <slint.h>
#include <spdlog/spdlog.h>

namespace radix_relay::slint_ui {

// Helper function for timestamp formatting
[[maybe_unused]] static auto format_timestamp() -> std::string
{
  using namespace std::chrono;

  auto now = system_clock::now();
  const std::time_t time_now = system_clock::to_time_t(now);

  std::tm time_tm{};
#if defined(_WIN32)
  std::ignore = localtime_s(&time_tm, &time_now);
#else
  std::ignore = localtime_r(&time_now, &time_tm);
#endif

  return fmt::format("{:02d}:{:02d}:{:02d}", time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec);
}

template<concepts::signal_bridge Bridge>
processor<Bridge>::processor(std::string node_id,
  std::string mode,
  const std::shared_ptr<Bridge> &bridge,
  const std::shared_ptr<async::async_queue<core::events::raw_command>> &command_queue,
  const std::shared_ptr<async::async_queue<core::events::display_message>> &display_queue)
  : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge), command_queue_(command_queue),
    display_queue_(display_queue), window_(std::make_shared<slint::ComponentHandle<MainWindow>>(MainWindow::create())),
    message_model_(std::make_shared<slint::VectorModel<Message>>())
{
  (*window_)->set_node_fingerprint(slint::SharedString(node_id_));
  (*window_)->set_current_mode(slint::SharedString(mode_));
  (*window_)->set_messages(message_model_);

  (*window_)->on_send_command([this](const slint::SharedString &command) {
    std::string cmd(command.data(), command.size());
    if (cmd.empty()) { return; }

    if (cmd == "quit" or cmd == "exit" or cmd == "q") {
      running_.store(false);
      (*window_)->hide();
      return;
    }

    command_queue_->push(core::events::raw_command{ .input = std::move(cmd) });
  });
}

template<concepts::signal_bridge Bridge> processor<Bridge>::~processor() { stop(); }

template<concepts::signal_bridge Bridge> auto processor<Bridge>::run() -> void
{
  running_.store(true);

  Message welcome_msg;
  welcome_msg.content = slint::SharedString("Radix Relay - Interactive Mode");
  welcome_msg.timestamp = slint::SharedString(format_timestamp());
  message_model_->push_back(welcome_msg);

  Message node_msg;
  node_msg.content = slint::SharedString(fmt::format("Node: {}", node_id_));
  node_msg.timestamp = slint::SharedString(format_timestamp());
  message_model_->push_back(node_msg);

  Message mode_msg;
  mode_msg.content = slint::SharedString(fmt::format("Mode: {}", mode_));
  mode_msg.timestamp = slint::SharedString(format_timestamp());
  message_model_->push_back(mode_msg);

  Message help_msg;
  help_msg.content = slint::SharedString("Type 'help' for available commands, 'quit' to exit");
  help_msg.timestamp = slint::SharedString(format_timestamp());
  message_model_->push_back(help_msg);

  setup_timer();

  (*window_)->run();

  stop();
}

template<concepts::signal_bridge Bridge> auto processor<Bridge>::stop() -> void
{
  if (running_.exchange(false)) {
    if (timer_) { timer_.reset(); }
    spdlog::debug("Slint UI processor stopped");
  }
}

template<concepts::signal_bridge Bridge> auto processor<Bridge>::setup_timer() -> void
{
  constexpr auto poll_interval_ms = 16;
  timer_ = std::make_shared<slint::Timer>();
  timer_->start(slint::TimerMode::Repeated, std::chrono::milliseconds(poll_interval_ms), [this]() {
    if (not running_.load()) { return; }
    poll_display_messages();
  });
}

template<concepts::signal_bridge Bridge> auto processor<Bridge>::poll_display_messages() -> void
{
  while (auto msg = display_queue_->try_pop()) {
    Message ui_msg;
    ui_msg.content = slint::SharedString(msg->message);
    ui_msg.timestamp = slint::SharedString(format_timestamp());
    message_model_->push_back(ui_msg);
  }
}

}// namespace radix_relay::slint_ui
