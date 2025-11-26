#pragma once

#include <core/events.hpp>

namespace radix_relay::concepts {

/**
 * @brief Concept defining the interface for handling raw command events.
 *
 * Types satisfying this concept must provide a handle() method for raw_command events.
 */
template<typename T>
concept event_handler = requires(T handler, const core::events::raw_command &raw_cmd) { handler.handle(raw_cmd); };

}// namespace radix_relay::concepts
