#pragma once

#include <radix_relay/concepts/nostr_message_handler.hpp>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <vector>

namespace radix_relay_test {

class TestDoubleNostrIncomingHandler
{
public:
  mutable std::vector<radix_relay::nostr::protocol::event_data> identity_events;
  mutable std::vector<radix_relay::nostr::protocol::event_data> encrypted_events;
  mutable std::vector<radix_relay::nostr::protocol::event_data> session_events;
  mutable std::vector<radix_relay::nostr::protocol::event_data> status_events;
  mutable std::vector<radix_relay::nostr::protocol::event_data> unknown_events;
  mutable std::vector<radix_relay::nostr::protocol::ok> ok_msgs;
  mutable std::vector<radix_relay::nostr::protocol::eose> eose_msgs;
  mutable std::vector<std::string> unknown_msgs;

  auto handle(const radix_relay::nostr::events::incoming::identity_announcement &event) const -> void
  {
    identity_events.push_back(event);
  }

  auto handle(const radix_relay::nostr::events::incoming::encrypted_message &event) const -> void
  {
    encrypted_events.push_back(event);
  }

  auto handle(const radix_relay::nostr::events::incoming::session_request &event) const -> void
  {
    session_events.push_back(event);
  }

  auto handle(const radix_relay::nostr::events::incoming::node_status &event) const -> void
  {
    status_events.push_back(event);
  }

  auto handle(const radix_relay::nostr::events::incoming::unknown_message &event) const -> void
  {
    unknown_events.push_back(event);
  }

  auto handle(const radix_relay::nostr::events::incoming::ok &event) const -> void { ok_msgs.push_back(event); }

  auto handle(const radix_relay::nostr::events::incoming::eose &event) const -> void { eose_msgs.push_back(event); }

  auto handle(const radix_relay::nostr::events::incoming::unknown_protocol &event) const -> void
  {
    unknown_msgs.push_back(event.message);
  }
};

static_assert(radix_relay::nostr::concepts::IncomingMessageHandler<TestDoubleNostrIncomingHandler>);

}// namespace radix_relay_test
