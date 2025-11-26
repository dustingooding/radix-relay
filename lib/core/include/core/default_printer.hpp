#pragma once

#include <fmt/core.h>

namespace radix_relay::core {

/**
 * @brief Default implementation of formatted console output.
 *
 * Provides a thin wrapper around fmt::print for outputting formatted text to stdout.
 */
class default_printer
{
public:
  /**
   * @brief Prints formatted output to stdout.
   *
   * @tparam Args Variadic template arguments for format string
   * @param format_string Format string in fmt library format
   * @param args Arguments to format into the string
   */
  template<typename... Args> auto print(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    fmt::print(format_string, std::forward<Args>(args)...);
  }
};

}// namespace radix_relay::core
