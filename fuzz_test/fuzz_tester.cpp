#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <core/cli.hpp>
#include <core/events.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <platform/env_utils.hpp>
#include <signal/signal_bridge.hpp>
#include <string>
#include <utility>

// Fuzzer that tests CLI command parsing and handling with arbitrary input
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) -> int
{
  using bridge_t = radix_relay::signal::bridge;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string input(reinterpret_cast<const char *>(Data), Size);

  auto db_path =
    (std::filesystem::path(radix_relay::platform::get_temp_directory()) / "fuzz_signal_bridge.db").string();
  auto bridge = std::make_shared<bridge_t>(db_path);

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);

  radix_relay::core::interactive_cli cli("fuzz-node", "hybrid", command_queue);

  std::ignore = cli.should_quit(input);
  std::ignore = cli.handle_command(input);

  // Cleanup: Remove the temporary database file
  std::ignore = std::filesystem::remove(db_path);

  return 0;
}
