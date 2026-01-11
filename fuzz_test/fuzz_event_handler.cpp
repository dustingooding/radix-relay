#include <core/contact_info.hpp>
#include <core/event_handler.hpp>
#include <core/events.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace {

struct noop_bridge
{
  [[nodiscard]] static auto lookup_contact(const std::string & /*contact*/) -> radix_relay::core::contact_info
  {
    return radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:test", .nostr_pubkey = "npub_test", .user_alias = "test", .has_active_session = false
    };
  }
};

struct noop_command_handler
{
  std::shared_ptr<noop_bridge> bridge = std::make_shared<noop_bridge>();

  // cppcheck-suppress functionStatic
  template<radix_relay::core::events::Command T> auto handle(const T & /*command*/) const -> void {}

  [[nodiscard]] auto get_bridge() const -> std::shared_ptr<noop_bridge> { return bridge; }
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
  const radix_relay::core::event_handler<noop_command_handler>::out_queues_t queues{};
  const radix_relay::core::event_handler<noop_command_handler> handler(command_handler, queues);

  handler.handle(radix_relay::core::events::raw_command{ .input = input });

  return 0;
}
