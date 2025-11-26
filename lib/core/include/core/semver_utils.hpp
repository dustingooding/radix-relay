#pragma once

#include <semver/semver.hpp>
#include <string>

namespace radix_relay::core {

/**
 * @brief Checks if a version meets a minimum version requirement.
 *
 * @param version_str Version string in semantic versioning format (e.g., "1.2.3")
 * @param minimum_version_str Minimum required version string
 * @return true if version >= minimum_version, false otherwise or on parse error
 */
[[nodiscard]] inline auto is_version_compatible(const std::string &version_str, const std::string &minimum_version_str)
  -> bool
{
  try {
    auto version = semver::version::parse(version_str);
    auto minimum_version = semver::version::parse(minimum_version_str);
    return version >= minimum_version;
  } catch (const semver::semver_exception &) {
    return false;
  }
}

}// namespace radix_relay::core
