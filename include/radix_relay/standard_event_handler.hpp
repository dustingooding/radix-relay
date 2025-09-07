#ifndef RADIX_RELAY_STANDARD_EVENT_HANDLER_HPP
#define RADIX_RELAY_STANDARD_EVENT_HANDLER_HPP

#include <radix_relay/command_handler.hpp>
#include <radix_relay/concepts/event_handler.hpp>
#include <radix_relay/event_handler.hpp>

namespace radix_relay {

using StandardEventHandler = EventHandler<CommandHandler>;

static_assert(concepts::EventHandler<StandardEventHandler>);

}// namespace radix_relay

#endif
