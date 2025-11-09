#pragma once

#include <fmt/core.h>
#include <functional>
#include <string>

namespace radix_relay::tui {

class printer
{
public:
  using message_callback_t = std::function<void(std::string)>;

  explicit printer(message_callback_t message_callback) : message_callback_(std::move(message_callback)) {}

  template<typename... Args> auto print(fmt::format_string<Args...> format_string, Args &&...args) const -> void
  {
    auto message = fmt::format(format_string, std::forward<Args>(args)...);
    if (message_callback_) { message_callback_(std::move(message)); }
  }

private:
  message_callback_t message_callback_;
};

}// namespace radix_relay::tui
