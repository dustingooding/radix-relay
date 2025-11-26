#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace radix_relay::nostr {

/**
 * @brief Extracts Radix protocol version from Nostr event tags.
 *
 * @param tags Event tags array
 * @return Version string if "radix_version" tag found, std::nullopt otherwise
 */
[[nodiscard]] inline auto extract_version_from_tags(const std::vector<std::vector<std::string>> &tags)
  -> std::optional<std::string>
{
  auto it = std::find_if(
    tags.begin(), tags.end(), [](const auto &tag) { return tag.size() >= 2 and tag[0] == "radix_version"; });

  if (it != tags.end()) { return (*it)[1]; }
  return std::nullopt;
}

}// namespace radix_relay::nostr
