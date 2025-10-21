#pragma once

#include <fmt/core.h>
#include <sstream>
#include <string>

namespace radix_relay_test {

class TestDoublePrinter
{
public:
  template<typename... Args> auto print(fmt::format_string<Args...> format_string, Args &&...args) -> void
  {
    output_ << fmt::format(format_string, std::forward<Args>(args)...);
  }

  [[nodiscard]] auto get_output() const -> std::string { return output_.str(); }

  auto clear_output() -> void { output_.str(""); }

private:
  mutable std::ostringstream output_;
};

}// namespace radix_relay_test
