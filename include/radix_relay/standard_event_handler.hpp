#pragma once

#include <radix_relay/command_handler.hpp>
#include <radix_relay/concepts/event_handler.hpp>
#include <radix_relay/event_handler.hpp>

namespace radix_relay {

using standard_event_handler_t = event_handler<command_handler>;

static_assert(concepts::event_handler<standard_event_handler_t>);

}// namespace radix_relay
