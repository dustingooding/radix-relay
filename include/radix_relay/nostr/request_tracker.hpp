#pragma once

#include <any>
#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <radix_relay/nostr/protocol.hpp>
#include <string>
#include <unordered_map>

namespace radix_relay::nostr {

class request_tracker
{
private:
  struct pending_request
  {
    std::function<void(const std::any &)> callback;
    std::shared_ptr<boost::asio::steady_timer> timer;
  };

public:
  explicit request_tracker(boost::asio::io_context *io_context) : io_context_(io_context) {}

  auto track(const std::string &event_id,
    std::function<void(const protocol::ok &)> callback,
    std::chrono::milliseconds timeout) -> void
  {
    auto timer = std::make_shared<boost::asio::steady_timer>(*io_context_, timeout);

    timer->async_wait([this, event_id](const boost::system::error_code &error) {
      if (!error) { handle_timeout(event_id); }
    });

    pending_[event_id] = pending_request{ .callback =
                                            [callback = std::move(callback)](const std::any &response_any) {
                                              callback(std::any_cast<const protocol::ok &>(response_any));
                                            },
      .timer = timer };
  }

  [[nodiscard]] auto has_pending(const std::string &event_id) const -> bool { return pending_.contains(event_id); }

  template<typename ResponseType> auto resolve(const std::string &event_id, const ResponseType &response) -> void
  {
    auto iter = pending_.find(event_id);
    if (iter != pending_.end()) {
      iter->second.timer->cancel();
      iter->second.callback(response);
      pending_.erase(iter);
    }
  }

  template<typename ResponseType = protocol::ok>
  [[nodiscard]] auto async_track(std::string event_id, std::chrono::milliseconds timeout)
    -> boost::asio::awaitable<ResponseType>
  {
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::cancellation_signal cancel_signal;
    auto result = std::make_shared<ResponseType>();
    auto event_done = std::make_shared<bool>(false);

    static constexpr int hours_per_day = 24;
    auto dummy_timer = std::make_shared<boost::asio::steady_timer>(*io_context_, std::chrono::hours(hours_per_day));
    pending_[event_id] = pending_request{ [&cancel_signal, result, event_done](const std::any &response_any) {
                                           *result = std::any_cast<ResponseType>(response_any);
                                           *event_done = true;
                                           cancel_signal.emit(boost::asio::cancellation_type::all);
                                         },
      dummy_timer };

    boost::asio::steady_timer timeout_timer(executor, timeout);

    boost::system::error_code error_code;
    co_await timeout_timer.async_wait(boost::asio::bind_cancellation_slot(
      cancel_signal.slot(), boost::asio::redirect_error(boost::asio::use_awaitable, error_code)));

    pending_.erase(event_id);

    if (error_code == boost::asio::error::operation_aborted and *event_done) { co_return *result; }

    throw std::runtime_error("Request timeout");
  }

private:
  auto handle_timeout(const std::string &event_id) -> void
  {
    auto iter = pending_.find(event_id);
    if (iter != pending_.end()) {
      protocol::ok timeout_response;
      timeout_response.event_id = event_id;
      timeout_response.accepted = false;
      timeout_response.message = "Request timeout";

      iter->second.callback(timeout_response);
      pending_.erase(iter);
    }
  }

  boost::asio::io_context *io_context_;
  std::unordered_map<std::string, pending_request> pending_;
};

}// namespace radix_relay::nostr
