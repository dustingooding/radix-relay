#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <radix_relay/cli.hpp>

// Fuzzer that tests CLI command parsing and handling with arbitrary input
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string input(reinterpret_cast<const char*>(Data), Size);
    
    radix_relay::InteractiveCli cli("fuzz-node", "hybrid");
    
    std::ignore = radix_relay::InteractiveCli::should_quit(input);
    std::ignore = cli.handle_command(input);
    
    return 0;
}
