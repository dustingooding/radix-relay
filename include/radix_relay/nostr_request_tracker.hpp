#pragma once

#include <any>
#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <radix_relay/nostr_protocol.hpp>
#include <string>
#include <unordered_map>

namespace radix_relay::nostr {

class RequestTracker
{
private:
  struct PendingRequest
  {
    std::function<void(const std::any &)> callback;
    std::shared_ptr<boost::asio::steady_timer> timer;
  };

public:
  explicit RequestTracker(boost::asio::io_context &io_context) : io_context_(io_context) {}

  auto track(std::string event_id,
    std::function<void(const protocol::ok &)> callback,
    std::chrono::milliseconds timeout) -> void
  {
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, timeout);

    timer->async_wait([this, event_id](const boost::system::error_code &error) {
      if (!error) { handle_timeout(event_id); }
    });

    pending_[event_id] = PendingRequest{ [callback = std::move(callback)](const std::any &response_any) {
                                          callback(std::any_cast<const protocol::ok &>(response_any));
                                        },
      timer };
  }

  [[nodiscard]] auto has_pending(const std::string &event_id) const -> bool { return pending_.contains(event_id); }

  template<typename ResponseType> auto resolve(const std::string &event_id, const ResponseType &response) -> void
  {
    auto it = pending_.find(event_id);
    if (it != pending_.end()) {
      it->second.timer->cancel();
      it->second.callback(response);
      pending_.erase(it);
    }
  }

  template<typename ResponseType = protocol::ok>
  auto async_track(std::string event_id, std::chrono::milliseconds timeout) -> boost::asio::awaitable<ResponseType>
  {
    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::cancellation_signal cancel_signal;
    auto result = std::make_shared<ResponseType>();
    auto event_done = std::make_shared<bool>(false);

    auto dummy_timer = std::make_shared<boost::asio::steady_timer>(io_context_, std::chrono::hours(24));
    pending_[event_id] = PendingRequest{ [&cancel_signal, result, event_done](const std::any &response_any) {
                                          *result = std::any_cast<ResponseType>(response_any);
                                          *event_done = true;
                                          cancel_signal.emit(boost::asio::cancellation_type::all);
                                        },
      dummy_timer };

    boost::asio::steady_timer timeout_timer(executor, timeout);

    boost::system::error_code ec;
    co_await timeout_timer.async_wait(boost::asio::bind_cancellation_slot(
      cancel_signal.slot(), boost::asio::redirect_error(boost::asio::use_awaitable, ec)));

    pending_.erase(event_id);

    if (ec == boost::asio::error::operation_aborted and *event_done) { co_return *result; }

    throw std::runtime_error("Request timeout");
  }

private:
  auto handle_timeout(const std::string &event_id) -> void
  {
    auto it = pending_.find(event_id);
    if (it != pending_.end()) {
      protocol::ok timeout_response;
      timeout_response.event_id = event_id;
      timeout_response.accepted = false;
      timeout_response.message = "Request timeout";

      it->second.callback(timeout_response);
      pending_.erase(it);
    }
  }

  boost::asio::io_context &io_context_;
  std::unordered_map<std::string, PendingRequest> pending_;
};

}// namespace radix_relay::nostr
