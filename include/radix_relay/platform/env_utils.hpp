#pragma once

#include <string>

namespace radix_relay::platform {

[[nodiscard]] auto get_home_directory() -> std::string;

[[nodiscard]] auto get_temp_directory() -> std::string;

[[nodiscard]] auto expand_tilde_path(const std::string &path) -> std::string;

}// namespace radix_relay::platform
