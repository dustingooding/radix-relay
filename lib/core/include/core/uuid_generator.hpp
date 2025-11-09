#pragma once

#include <string>

namespace radix_relay::core {

class uuid_generator
{
public:
  [[nodiscard]] static auto generate() -> std::string;
};

}// namespace radix_relay::core
