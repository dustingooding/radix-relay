#pragma once

#include <radix_relay/command_handler.hpp>
#include <radix_relay/concepts/event_handler.hpp>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/event_handler.hpp>

namespace radix_relay {

template<concepts::signal_bridge Bridge> using standard_event_handler_t = event_handler<command_handler<Bridge>>;

}// namespace radix_relay
