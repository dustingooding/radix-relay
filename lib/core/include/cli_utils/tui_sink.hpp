#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <memory>
#include <spdlog/sinks/base_sink.h>
#include <string>

namespace radix_relay::cli_utils {

template<typename Mutex> class tui_sink final : public spdlog::sinks::base_sink<Mutex>
{
public:
  explicit tui_sink(std::shared_ptr<async::async_queue<core::events::display_message>> queue)
    : display_queue_(std::move(queue))
  {}

protected:
  auto sink_it_(const spdlog::details::log_msg &msg) -> void override
  {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
    std::string message(formatted.data(), formatted.size());

    if (not message.empty() and message.back() == '\n') { message.pop_back(); }

    if (display_queue_) { display_queue_->push(core::events::display_message{ .message = message }); }
  }

  auto flush_() -> void override {}

private:
  std::shared_ptr<async::async_queue<core::events::display_message>> display_queue_;
};

using tui_sink_mutex_t = tui_sink<std::mutex>;

}// namespace radix_relay::cli_utils
