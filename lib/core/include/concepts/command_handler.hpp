#pragma once

#include <core/events.hpp>

namespace radix_relay::concepts {

/**
 * @brief Concept defining the interface for handling CLI commands.
 *
 * Types satisfying this concept must provide handle() overloads for all core command event types.
 */
template<typename T>
concept command_handler = requires(T handler,
  const core::events::help &help_cmd,
  const core::events::peers &peers_cmd,
  const core::events::status &status_cmd,
  const core::events::sessions &sessions_cmd,
  const core::events::scan &scan_cmd,
  const core::events::version &version_cmd,
  const core::events::mode &mode_cmd,
  const core::events::send &send_cmd,
  const core::events::broadcast &broadcast_cmd,
  const core::events::connect &connect_cmd,
  const core::events::trust &trust_cmd,
  const core::events::verify &verify_cmd,
  const core::events::chat &chat_cmd,
  const core::events::leave &leave_cmd) {
  handler.handle(help_cmd);
  handler.handle(peers_cmd);
  handler.handle(status_cmd);
  handler.handle(sessions_cmd);
  handler.handle(scan_cmd);
  handler.handle(version_cmd);
  handler.handle(mode_cmd);
  handler.handle(send_cmd);
  handler.handle(broadcast_cmd);
  handler.handle(connect_cmd);
  handler.handle(trust_cmd);
  handler.handle(verify_cmd);
  handler.handle(chat_cmd);
  handler.handle(leave_cmd);
};

}// namespace radix_relay::concepts
