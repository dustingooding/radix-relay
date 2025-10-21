#pragma once

#include <string>

namespace radix_relay::concepts {

template<typename T>
concept printer = requires(T printer, const std::string &format_string) {
  // Print formatted strings
  { printer.print(format_string) } -> std::convertible_to<void>;
  { printer.print(format_string, "arg") } -> std::convertible_to<void>;
};

}// namespace radix_relay::concepts
