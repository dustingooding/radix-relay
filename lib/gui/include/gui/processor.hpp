#pragma once

#include <async/async_queue.hpp>
#include <atomic>
#include <chrono>
#include <concepts/signal_bridge.hpp>
#include <core/contact_info.hpp>
#include <core/events.hpp>
#include <core/overload.hpp>
#include <fmt/format.h>
#include <main_window.h>
#include <memory>
#include <optional>
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
    const std::shared_ptr<async::async_queue<core::events::ui_event_t>> &ui_event_queue,
    const slint::ComponentHandle<MainWindow> &window,
    const std::shared_ptr<slint::VectorModel<Message>> &message_model)
    : node_id_(std::move(node_id)), mode_(std::move(mode)), bridge_(bridge), command_queue_(command_queue),
      ui_event_queue_(ui_event_queue), window_(window), message_model_(message_model),
      contact_list_model_(std::make_shared<slint::VectorModel<Contact>>())
  {
    window_->set_node_fingerprint(slint::SharedString(node_id_));
    window_->set_current_mode(slint::SharedString(mode_));
    window_->set_messages(message_model_);

    auto contacts = bridge_->list_contacts();
    for (const auto &contact : contacts) {
      if (contact.user_alias == "self") { continue; }

      Contact ui_contact;
      ui_contact.rdx_fingerprint = slint::SharedString(contact.rdx_fingerprint);
      ui_contact.user_alias = slint::SharedString(contact.user_alias);
      ui_contact.has_active_session = contact.has_active_session;
      contact_list_model_->push_back(ui_contact);
    }

    window_->set_contacts(contact_list_model_);

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

  auto poll_ui_events() -> void
  {
    constexpr std::size_t max_events_per_poll = 10;
    std::size_t processed = 0;

    while (processed < max_events_per_poll) {
      auto event = ui_event_queue_->try_pop();
      if (not event.has_value()) { break; }

      process_ui_event(*event);
      ++processed;
    }
  }

  [[nodiscard]] auto get_message_model() const -> std::shared_ptr<slint::VectorModel<Message>>
  {
    return message_model_;
  }

  [[nodiscard]] auto get_contact_list_model() const -> std::shared_ptr<slint::VectorModel<Contact>>
  {
    return contact_list_model_;
  }

  [[nodiscard]] auto is_running() const -> bool { return running_.load(); }

  auto update_chat_context(const std::string &contact_name) -> void
  {
    active_chat_context_ = contact_name;
    window_->set_active_chat_contact(slint::SharedString(contact_name));
  }

  auto clear_chat_context() -> void
  {
    active_chat_context_.reset();
    window_->set_active_chat_contact(slint::SharedString(""));
  }

  [[nodiscard]] auto get_chat_context() const -> std::optional<std::string> { return active_chat_context_; }

private:
  auto setup_timer() -> void
  {
    constexpr auto poll_interval_ms = 16;
    timer_ = std::make_shared<slint::Timer>();
    timer_->start(slint::TimerMode::Repeated, std::chrono::milliseconds(poll_interval_ms), [this]() {
      if (not running_.load()) { return; }
      poll_ui_events();
    });
  }

  auto process_ui_event(const core::events::ui_event_t &event) -> void
  {
    std::visit(core::overload{ [this](const core::events::display_message &evt) {
                                auto content = evt.message;
                                content.erase(content.find_last_not_of("\r\n") + 1);

                                Message ui_msg;
                                ui_msg.content = slint::SharedString(content);
                                ui_msg.timestamp = slint::SharedString(platform::format_current_time_hms());
                                message_model_->push_back(ui_msg);
                              },
                 [this](const core::events::enter_chat_mode &evt) { update_chat_context(evt.display_name); },
                 [this](const core::events::exit_chat_mode &) { clear_chat_context(); } },
      event);
  }

  std::string node_id_;
  std::string mode_;
  std::shared_ptr<Bridge> bridge_;
  std::shared_ptr<async::async_queue<core::events::raw_command>> command_queue_;
  std::shared_ptr<async::async_queue<core::events::ui_event_t>> ui_event_queue_;

  std::atomic<bool> running_{ false };

  slint::ComponentHandle<MainWindow> window_;
  std::shared_ptr<slint::VectorModel<Message>> message_model_;
  std::shared_ptr<slint::VectorModel<Contact>> contact_list_model_;
  std::shared_ptr<slint::Timer> timer_;
  std::optional<std::string> active_chat_context_;
};

[[nodiscard]] inline auto make_window() -> slint::ComponentHandle<MainWindow> { return MainWindow::create(); }

[[nodiscard]] inline auto make_message_model() -> std::shared_ptr<slint::VectorModel<Message>>
{
  return std::make_shared<slint::VectorModel<Message>>();
}

}// namespace radix_relay::gui
