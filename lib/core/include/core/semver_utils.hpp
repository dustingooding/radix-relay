#pragma once

#include <semver/semver.hpp>
#include <string>

namespace radix_relay::core {

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
