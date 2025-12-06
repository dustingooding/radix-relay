#pragma once

#include <string>

namespace radix_relay::platform {

/**
 * @brief Formats the current local time as HH:MM:SS.
 *
 * @return Current time formatted as "HH:MM:SS"
 */
[[nodiscard]] auto format_current_time_hms() -> std::string;

}// namespace radix_relay::platform
