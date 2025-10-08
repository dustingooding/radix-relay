#pragma once

#include <concepts>
#include <functional>
#include <radix_relay/events/nostr_events.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <string>

namespace radix_relay::nostr::concepts {

template<typename T>
concept NostrHandler = requires(T handler,
  const events::incoming::identity_announcement &identity_event,
  const events::incoming::encrypted_message &encrypted_event,
  const events::incoming::session_request &session_event,
  const events::incoming::node_status &status_event,
  const events::incoming::unknown_message &unknown_event,
  const events::incoming::ok &ok_event,
  const events::incoming::eose &eose_event,
  const events::incoming::unknown_protocol &unknown_protocol_event,
  const events::outgoing::identity_announcement &outgoing_identity_event,
  const events::outgoing::encrypted_message &outgoing_encrypted_event,
  const events::outgoing::session_request &outgoing_session_event,
  const events::outgoing::plaintext_message &plaintext_event,
  const events::outgoing::subscription_request &subscription_event,
  const std::function<void(const std::string &)> &track_fn) {
  handler.handle(identity_event);
  handler.handle(encrypted_event);
  handler.handle(session_event);
  handler.handle(status_event);
  handler.handle(unknown_event);
  handler.handle(ok_event);
  handler.handle(eose_event);
  handler.handle(unknown_protocol_event);
  handler.handle(outgoing_identity_event, track_fn);
  handler.handle(outgoing_encrypted_event, track_fn);
  handler.handle(outgoing_session_event, track_fn);
  handler.handle(plaintext_event, track_fn);
  handler.handle(subscription_event);
};

}// namespace radix_relay::nostr::concepts
