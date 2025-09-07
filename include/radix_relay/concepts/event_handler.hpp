#ifndef RADIX_RELAY_EVENT_HANDLER_CONCEPT_HPP
#define RADIX_RELAY_EVENT_HANDLER_CONCEPT_HPP

#include <concepts>
#include <radix_relay/events/events.hpp>

namespace radix_relay::concepts {

template<typename T>
concept EventHandler = requires(T handler, const events::raw_command &raw_cmd) { handler.handle(raw_cmd); };

}// namespace radix_relay::concepts

#endif
