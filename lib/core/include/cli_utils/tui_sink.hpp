#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <memory>
#include <platform/time_utils.hpp>
#include <spdlog/sinks/base_sink.h>
#include <string>

namespace radix_relay::cli_utils {

/**
 * @brief Custom spdlog sink that routes log messages to a display queue.
 *
 * @tparam Mutex Mutex type for thread safety (e.g., std::mutex)
 *
 * Formats log messages and pushes them to an async queue for display in the TUI.
 */
template<typename Mutex> class tui_sink final : public spdlog::sinks::base_sink<Mutex>
{
public:
  /**
   * @brief Constructs a TUI sink with the given display queue.
   *
   * @param queue Queue for outgoing display messages
   */
  explicit tui_sink(std::shared_ptr<async::async_queue<core::events::display_filter_input_t>> queue)
    : display_queue_(std::move(queue))
  {}

protected:
  /**
   * @brief Formats and queues a log message.
   *
   * @param msg Log message from spdlog
   */
  auto sink_it_(const spdlog::details::log_msg &msg) -> void override
  {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
    std::string message(formatted.data(), formatted.size());

    if (not message.empty() and message.back() == '\n') { message.pop_back(); }

    if (display_queue_) {
      display_queue_->push(core::events::display_message{ .message = message,
        .contact_rdx = std::nullopt,
        .timestamp = platform::current_timestamp_ms(),
        .source_type = core::events::display_message::source::system });
    }
  }

  /**
   * @brief Flushes pending log messages (no-op for queue-based sink).
   */
  auto flush_() -> void override {}

private:
  std::shared_ptr<async::async_queue<core::events::display_filter_input_t>> display_queue_;
};

/// Type alias for mutex-protected TUI sink
using tui_sink_mutex_t = tui_sink<std::mutex>;

}// namespace radix_relay::cli_utils
