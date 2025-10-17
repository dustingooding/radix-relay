#pragma once

#include <radix_relay/events/nostr_events.hpp>
#include <type_traits>

namespace radix_relay::concepts {

template<typename T>
concept HandlerTrackedEvent =
  std::is_base_of_v<nostr::protocol::event_data, T> || std::is_same_v<T, nostr::events::outgoing::plaintext_message>;

}// namespace radix_relay::concepts
