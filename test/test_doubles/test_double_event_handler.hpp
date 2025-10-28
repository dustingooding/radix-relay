#pragma once

#include <algorithm>
#include <radix_relay/concepts/event_handler.hpp>
#include <radix_relay/core/events.hpp>
#include <string>
#include <vector>

namespace radix_relay_test {

struct test_double_event_handler
{
  mutable std::vector<std::string> handled_raw_commands;

  auto handle(const radix_relay::core::events::raw_command &event) const -> void
  {
    handled_raw_commands.push_back(event.input);
  }

  auto was_handled(const std::string &input) const -> bool
  {
    return std::any_of(handled_raw_commands.cbegin(),
      handled_raw_commands.cend(),
      [&input](const std::string &handled) { return handled == input; });
  }

  auto get_handle_count() const -> size_t { return handled_raw_commands.size(); }

  auto clear_handles() const -> void { handled_raw_commands.clear(); }

  auto get_last_handled() const -> std::string
  {
    return handled_raw_commands.empty() ? "" : handled_raw_commands.back();
  }
};

static_assert(radix_relay::concepts::event_handler<test_double_event_handler>);

}// namespace radix_relay_test
