#pragma once

#include <core/events.hpp>

namespace radix_relay::concepts {

template<typename T>
concept event_handler = requires(T handler, const core::events::raw_command &raw_cmd) { handler.handle(raw_cmd); };

}// namespace radix_relay::concepts
