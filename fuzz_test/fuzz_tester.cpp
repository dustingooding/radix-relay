#include <cstddef>
#include <cstdint>
#include <radix_relay/cli.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <string>
#include <utility>

// Fuzzer that tests CLI command parsing and handling with arbitrary input
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
extern "C" auto LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) -> int
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string input(reinterpret_cast<const char *>(Data), Size);

  const radix_relay::StandardEventHandler::command_handler_t command_handler;
  const radix_relay::StandardEventHandler event_handler{ command_handler };
  radix_relay::InteractiveCli cli("fuzz-node", "hybrid", event_handler);

  using CliType = decltype(cli);
  std::ignore = CliType::should_quit(input);
  std::ignore = cli.handle_command(input);

  return 0;
}
