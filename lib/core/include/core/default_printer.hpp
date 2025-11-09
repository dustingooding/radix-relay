#pragma once

#include <fmt/core.h>

namespace radix_relay::core {

class default_printer
{
public:
  template<typename... Args> auto print(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    fmt::print(format_string, std::forward<Args>(args)...);
  }
};

}// namespace radix_relay::core
