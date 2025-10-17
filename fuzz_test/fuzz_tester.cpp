#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <radix_relay/cli.hpp>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <string>
#include <utility>

// Fuzzer that tests CLI command parsing and handling with arbitrary input
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) -> int
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string input(reinterpret_cast<const char *>(Data), Size);

  auto db_path =
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "fuzz_signal_bridge.db").string();
  auto bridge = radix_relay::new_signal_bridge(db_path.c_str());
  auto command_handler = std::make_shared<radix_relay::standard_event_handler_t::command_handler_t>(std::move(bridge));
  auto event_handler = std::make_shared<radix_relay::standard_event_handler_t>(command_handler);
  radix_relay::interactive_cli cli("fuzz-node", "hybrid", event_handler);

  using cli_type_t = decltype(cli);
  std::ignore = cli_type_t::should_quit(input);
  std::ignore = cli.handle_command(input);

  // Cleanup: Remove the temporary database file
  std::ignore = std::filesystem::remove(db_path);

  return 0;
}
