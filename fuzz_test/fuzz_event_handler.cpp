#include <core/event_handler.hpp>
#include <core/events.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace {

struct noop_command_handler
{
  // cppcheck-suppress functionStatic
  template<radix_relay::core::events::Command T> auto handle(const T & /*command*/) const -> void {}
};

}// namespace

// Fuzzer that tests event_handler command parsing with arbitrary input
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) -> int
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string input(reinterpret_cast<const char *>(Data), Size);

  auto command_handler = std::make_shared<noop_command_handler>();
  const radix_relay::core::event_handler<noop_command_handler> handler(command_handler);

  handler.handle(radix_relay::core::events::raw_command{ .input = input });

  return 0;
}
