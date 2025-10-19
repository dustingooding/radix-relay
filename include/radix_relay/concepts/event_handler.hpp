#pragma once

#include <radix_relay/events/events.hpp>

namespace radix_relay::concepts {

template<typename T>
concept event_handler = requires(T handler, const events::raw_command &raw_cmd) { handler.handle(raw_cmd); };

}// namespace radix_relay::concepts
