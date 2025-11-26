#pragma once

#include <string>

namespace radix_relay::platform {

/**
 * @brief Returns the user's home directory path.
 *
 * @return Home directory path
 */
[[nodiscard]] auto get_home_directory() -> std::string;

/**
 * @brief Returns the system's temporary directory path.
 *
 * @return Temporary directory path
 */
[[nodiscard]] auto get_temp_directory() -> std::string;

/**
 * @brief Expands tilde (~) in path to home directory.
 *
 * @param path Path possibly containing ~
 * @return Expanded absolute path
 */
[[nodiscard]] auto expand_tilde_path(const std::string &path) -> std::string;

}// namespace radix_relay::platform
