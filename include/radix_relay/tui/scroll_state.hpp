#pragma once

namespace radix_relay::tui {

class scroll_state
{
public:
  scroll_state() = default;

  [[nodiscard]] auto should_stick_to_bottom() const -> bool { return not user_has_scrolled_up_; }

  auto handle_wheel_up() -> void { user_has_scrolled_up_ = true; }

  auto handle_end_key() -> void { user_has_scrolled_up_ = false; }

  auto reset_to_bottom() -> void { user_has_scrolled_up_ = false; }

private:
  bool user_has_scrolled_up_ = false;
};

}// namespace radix_relay::tui
