#pragma once

#include <concepts/event_handler.hpp>
#include <concepts/signal_bridge.hpp>
#include <core/command_handler.hpp>
#include <core/event_handler.hpp>

namespace radix_relay::core {

template<concepts::signal_bridge Bridge> using standard_event_handler_t = event_handler<command_handler<Bridge>>;

}// namespace radix_relay::core
