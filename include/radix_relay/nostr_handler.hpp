#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <radix_relay/concepts/nostr_message_handler.hpp>
#include <radix_relay/concepts/transport.hpp>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <ranges>
#include <span>
#include <string>

namespace radix_relay::nostr {

template<concepts::IncomingMessageHandler Handler> class Dispatcher
{
private:
  Handler &handler_;

  auto dispatch_event(const protocol::event_data &event) -> void
  {
    const auto kind = event.get_kind();
    if (!kind.has_value()) {
      handler_.handle(events::incoming::unknown_message{ event });
      return;
    }

    switch (kind.value()) {
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
  explicit Dispatcher(Handler &handler) : handler_(handler) {}

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
          handler_.handle(events::incoming::ok{ *ok_msg });
        } else {
          handler_.handle(events::incoming::unknown_protocol{ json_str });
        }
      } else if (msg_type == "EOSE") {
        auto eose_msg = protocol::eose::deserialize(json_str);
        if (eose_msg) {
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

template<radix_relay::concepts::Transport Transport> class OutgoingHandler
{
private:
  Transport &transport_;

public:
  explicit OutgoingHandler(Transport &transport) : transport_(transport) {}

  auto handle(const events::outgoing::identity_announcement &event) -> void { send_event(event); }

  auto handle(const events::outgoing::encrypted_message &event) -> void { send_event(event); }

  auto handle(const events::outgoing::session_request &event) -> void { send_event(event); }

private:
  auto send_event(const protocol::event_data &event) -> void
  {
    auto protocol_event = protocol::event::from_event_data(event);
    auto json_str = protocol_event.serialize();

    std::vector<std::byte> bytes;
    bytes.resize(json_str.size());
    std::ranges::transform(json_str, bytes.begin(), [](char c) { return std::bit_cast<std::byte>(c); });
    transport_.send(bytes);
  }
};

}// namespace radix_relay::nostr
