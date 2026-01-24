#include <concepts/signal_bridge.hpp>
#include <core/command_parser.hpp>
#include <core/contact_info.hpp>
#include <core/event_handler.hpp>
#include <core/events.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <signal_types/signal_types.hpp>
#include <string>
#include <vector>

namespace {

/**
 * @brief Minimal bridge that satisfies signal_bridge concept for fuzzing.
 */
struct noop_bridge
{
  [[nodiscard]] static auto get_node_fingerprint() -> std::string { return "RDX:fuzz_node"; }

  [[nodiscard]] static auto list_contacts() -> std::vector<radix_relay::core::contact_info> { return {}; }

  [[nodiscard]] static auto lookup_contact(const std::string & /*contact*/) -> radix_relay::core::contact_info
  {
    return radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:test", .nostr_pubkey = "npub_test", .user_alias = "test", .has_active_session = false
    };
  }

  static auto encrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) -> std::vector<uint8_t>
  {
    return bytes;
  }

  static auto decrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes)
    -> radix_relay::signal::decryption_result
  {
    return { .plaintext = bytes, .should_republish_bundle = false };
  }

  static auto add_contact_and_establish_session_from_base64(const std::string & /*bundle*/,
    const std::string & /*alias*/) -> std::string
  {
    return "RDX:new_contact";
  }

  static auto generate_prekey_bundle_announcement(const std::string & /*version*/) -> radix_relay::signal::bundle_info
  {
    return { .announcement_json = "{}", .pre_key_id = 1, .signed_pre_key_id = 1, .kyber_pre_key_id = 1 };
  }

  static auto generate_empty_bundle_announcement(const std::string & /*version*/) -> std::string { return "{}"; }

  static auto extract_rdx_from_bundle_base64(const std::string & /*bundle*/) -> std::string { return "RDX:extracted"; }

  static auto assign_contact_alias(const std::string & /*rdx*/, const std::string & /*alias*/) -> void {}

  static auto create_and_sign_encrypted_message(const std::string & /*rdx*/,
    const std::string & /*content*/,
    uint32_t /*timestamp*/,
    const std::string & /*version*/) -> std::string
  {
    return "{}";
  }

  static auto sign_nostr_event(const std::string & /*event*/) -> std::string { return "{}"; }

  static auto create_subscription_for_self(const std::string &sub_id, std::uint64_t /*since*/ = 0) -> std::string
  {
    return R"(["REQ",")" + sub_id + R"(",{}])";
  }

  static auto update_last_message_timestamp(std::uint64_t /*timestamp*/) -> void {}

  static auto perform_key_maintenance() -> radix_relay::signal::key_maintenance_result
  {
    return { .signed_pre_key_rotated = false, .kyber_pre_key_rotated = false, .pre_keys_replenished = false };
  }

  static auto record_published_bundle(std::uint32_t /*pre_key_id*/,
    std::uint32_t /*signed_pre_key_id*/,
    std::uint32_t /*kyber_pre_key_id*/) -> void
  {}

  static auto get_conversation_messages(const std::string & /*rdx*/, std::uint32_t /*limit*/, std::uint32_t /*offset*/)
    -> std::vector<radix_relay::signal::stored_message>
  {
    return {};
  }

  static auto mark_conversation_read(const std::string & /*rdx*/) -> void {}

  static auto mark_conversation_read_up_to(const std::string & /*rdx*/, std::uint64_t /*timestamp*/) -> void {}

  static auto get_unread_count(const std::string & /*rdx*/) -> std::uint32_t { return 0; }

  static auto get_conversations(bool /*include_archived*/) -> std::vector<radix_relay::signal::conversation>
  {
    return {};
  }

  static auto delete_message(std::int64_t /*message_id*/) -> void {}

  static auto delete_conversation(const std::string & /*rdx*/) -> void {}
};

static_assert(radix_relay::concepts::signal_bridge<noop_bridge>);

/**
 * @brief No-op command handler for fuzzing - does nothing with commands.
 */
struct noop_command_handler
{
  auto operator()(const radix_relay::core::events::help & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::peers & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::status & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::sessions & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::scan & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::version & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::identities & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::publish_identity & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::unpublish_identity & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::mode & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::send & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::broadcast & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::connect & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::disconnect & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::trust & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::verify & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::chat & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::leave & /*cmd*/) const -> void {}
  auto operator()(const radix_relay::core::events::unknown_command & /*cmd*/) const -> void {}
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
  auto bridge = std::make_shared<noop_bridge>();
  auto parser = std::make_shared<radix_relay::core::command_parser<noop_bridge>>(bridge);

  using handler_t =
    radix_relay::core::event_handler<noop_command_handler, radix_relay::core::command_parser<noop_bridge>>;
  const handler_t::out_queues_t queues{};
  const handler_t handler(command_handler, parser, queues);

  handler.handle(radix_relay::core::events::raw_command{ .input = input });

  return 0;
}
