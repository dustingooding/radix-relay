#pragma once

#include <concepts>
#include <radix_relay/events/events.hpp>

namespace radix_relay::concepts {

template<typename T>
concept CommandHandler = requires(T handler,
  const events::help &help_cmd,
  const events::peers &peers_cmd,
  const events::status &status_cmd,
  const events::sessions &sessions_cmd,
  const events::scan &scan_cmd,
  const events::version &version_cmd,
  const events::mode &mode_cmd,
  const events::send &send_cmd,
  const events::broadcast &broadcast_cmd,
  const events::connect &connect_cmd,
  const events::trust &trust_cmd,
  const events::verify &verify_cmd) {
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
};

}// namespace radix_relay::concepts
