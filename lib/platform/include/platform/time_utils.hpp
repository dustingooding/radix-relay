#pragma once

#include <cstdint>
#include <string>

namespace radix_relay::platform {

/**
 * @brief Formats the current local time as HH:MM:SS.
 *
 * @return Current time formatted as "HH:MM:SS"
 */
[[nodiscard]] auto format_current_time_hms() -> std::string;

/**
 * @brief Gets current timestamp in milliseconds since Unix epoch.
 *
 * @return Current time as milliseconds since epoch (UTC)
 */
[[nodiscard]] auto current_timestamp_ms() -> std::uint64_t;

}// namespace radix_relay::platform
