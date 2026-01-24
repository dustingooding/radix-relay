#pragma once

#include <concepts/signal_bridge.hpp>
#include <core/command_handler.hpp>
#include <core/event_handler.hpp>

namespace radix_relay::core {

/**
 * @brief Type alias for the standard event handler using command_handler.
 *
 * @tparam Bridge Type satisfying the signal_bridge concept
 *
 * Combines event_handler with command_handler to create a complete command processing pipeline.
 */
template<concepts::signal_bridge Bridge>
using standard_event_handler_t = event_handler<command_handler<Bridge>, Bridge>;

}// namespace radix_relay::core
