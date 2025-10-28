#pragma once

#include <string>

namespace radix_relay::transport {

class uuid_generator
{
public:
  [[nodiscard]] static auto generate() -> std::string;
};

}// namespace radix_relay::transport
