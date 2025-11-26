#pragma once

#include <string>

namespace radix_relay::core {

/**
 * @brief Utility for generating RFC 4122 UUIDs.
 */
class uuid_generator
{
public:
  /**
   * @brief Generates a new UUID string.
   *
   * @return UUID in canonical format (e.g., "550e8400-e29b-41d4-a716-446655440000")
   */
  [[nodiscard]] static auto generate() -> std::string;
};

}// namespace radix_relay::core
