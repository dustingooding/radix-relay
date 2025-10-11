#pragma once

#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include <radix_relay/concepts/nostr_message_handler.hpp>
#include <radix_relay/concepts/request_tracker.hpp>
#include <radix_relay/concepts/trackable_event.hpp>
#include <radix_relay/concepts/transport.hpp>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <radix_relay/nostr_request_tracker.hpp>
#include <ranges>
#include <span>
#include <string>

namespace radix_relay::nostr {

template<concepts::NostrHandler Handler, radix_relay::concepts::RequestTracker Tracker> class Dispatcher
{
private:
  Handler &handler_;
  Tracker &tracker_;

  auto dispatch_event(const protocol::event_data &event) -> void
  {
    const auto kind = event.get_kind();
    if (!kind.has_value()) {
      handler_.handle(events::incoming::unknown_message{ event });
      return;
    }

    switch (kind.value()) {
    case protocol::kind::bundle_announcement:
      handler_.handle(events::incoming::bundle_announcement{ event });
      break;
    case protocol::kind::identity_announcement:
      handler_.handle(events::incoming::identity_announcement{ event });
      break;
    case protocol::kind::encrypted_message:
      handler_.handle(events::incoming::encrypted_message{ event });
      break;
    case protocol::kind::session_request:
      handler_.handle(events::incoming::session_request{ event });
      break;
    case protocol::kind::node_status:
      handler_.handle(events::incoming::node_status{ event });
      break;
    case protocol::kind::parameterized_replaceable_start:
    case protocol::kind::profile_metadata:
    case protocol::kind::text_note:
    case protocol::kind::recommend_relay:
    case protocol::kind::contact_list:
    case protocol::kind::encrypted_dm:
    case protocol::kind::reaction:
      handler_.handle(events::incoming::unknown_message{ event });
      break;
    }
  }

public:
  Dispatcher(Handler &handler, Tracker &tracker) : handler_(handler), tracker_(tracker) {}

  auto dispatch_bytes(std::span<const std::byte> raw_bytes) -> void
  {
    std::string json_str;
    json_str.resize(raw_bytes.size());
    std::ranges::transform(raw_bytes, json_str.begin(), [](std::byte b) { return std::bit_cast<char>(b); });

    try {
      auto j = nlohmann::json::parse(json_str);
      if (!j.is_array() || j.empty() || !j[0].is_string()) {
        handler_.handle(events::incoming::unknown_protocol{ json_str });
        return;
      }

      const auto msg_type = j[0].get<std::string>();

      if (msg_type == "OK") {
        auto ok_msg = protocol::ok::deserialize(json_str);
        if (ok_msg) {
          tracker_.resolve(ok_msg->event_id, *ok_msg);
          handler_.handle(events::incoming::ok{ *ok_msg });
        } else {
          handler_.handle(events::incoming::unknown_protocol{ json_str });
        }
      } else if (msg_type == "EOSE") {
        auto eose_msg = protocol::eose::deserialize(json_str);
        if (eose_msg) {
          tracker_.resolve(eose_msg->subscription_id, *eose_msg);
          handler_.handle(events::incoming::eose{ *eose_msg });
        } else {
          handler_.handle(events::incoming::unknown_protocol{ json_str });
        }
      } else if (msg_type == "EVENT") {
        auto event_msg = protocol::event::deserialize(json_str);
        if (event_msg) {
          dispatch_event(event_msg->data);
        } else {
          handler_.handle(events::incoming::unknown_protocol{ json_str });
        }
      } else {
        handler_.handle(events::incoming::unknown_protocol{ json_str });
      }
    } catch (const std::exception &) {
      handler_.handle(events::incoming::unknown_protocol{ json_str });
    }
  }

  auto create_transport_callback() -> std::function<void(std::span<const std::byte>)>
  {
    return [this](std::span<const std::byte> raw_bytes) { dispatch_bytes(raw_bytes); };
  }
};

template<concepts::NostrHandler Handler, radix_relay::concepts::Transport Transport> class Session
{
private:
  std::reference_wrapper<Handler> handler_;
  RequestTracker tracker_;
  Dispatcher<Handler, RequestTracker> dispatcher_;

public:
  explicit Session(Transport &transport, Handler &handler)
    : handler_(handler), tracker_(transport.io_context()), dispatcher_(handler, tracker_)
  {
    transport.register_message_callback(dispatcher_.create_transport_callback());
  }

  auto handle(const auto &event) -> void { handler_.get().handle(event); }

  template<typename ResponseType = protocol::ok>
  auto handle(const radix_relay::concepts::HandlerTrackedEvent auto &event, std::chrono::milliseconds timeout)
    -> boost::asio::awaitable<ResponseType>
  {
    auto awaitable_tracker = std::make_shared<std::optional<std::string>>();

    auto track_fn = [awaitable_tracker](const std::string &event_id) { *awaitable_tracker = event_id; };

    handler_.get().handle(event, track_fn);

    if (!awaitable_tracker->has_value()) { throw std::runtime_error("Handler did not call track_fn"); }

    co_return co_await tracker_.async_track<ResponseType>(awaitable_tracker->value(), timeout);
  }

  auto handle(const events::outgoing::subscription_request &event, std::chrono::milliseconds timeout)
    -> boost::asio::awaitable<protocol::eose>
  {
    const std::string subscription_id = event.get_subscription_id();

    handler_.get().handle(event);

    co_return co_await tracker_.async_track<protocol::eose>(subscription_id, timeout);
  }
};

}// namespace radix_relay::nostr
