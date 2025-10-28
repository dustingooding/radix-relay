#pragma once

#include <cstdint>
#include <filesystem>
#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/signal/node_identity.hpp>
#include <radix_relay/signal/signal_types.hpp>
#include <rust/cxx.h>
#include <signal_bridge_cxx/lib.h>
#include <string>
#include <vector>

namespace radix_relay::signal {

class bridge
{
public:
  explicit bridge(const std::filesystem::path &bridge_db)
    : bridge_(radix_relay::new_signal_bridge(bridge_db.string().c_str()))
  {}
  explicit bridge(rust::Box<SignalBridge> signal_bridge) : bridge_(std::move(signal_bridge)) {}

  bridge(const bridge &) = delete;
  auto operator=(const bridge &) -> bridge & = delete;
  bridge(bridge &&) = default;
  auto operator=(bridge &&) -> bridge & = default;
  ~bridge() = default;

  [[nodiscard]] auto get_node_fingerprint() const -> std::string;

  [[nodiscard]] auto list_contacts() const -> std::vector<contact_info>;

  [[nodiscard]] auto encrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const
    -> std::vector<uint8_t>;

  [[nodiscard]] auto decrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const
    -> std::vector<uint8_t>;

  [[nodiscard]] auto add_contact_and_establish_session_from_base64(const std::string &bundle,
    const std::string &alias) const -> std::string;

  [[nodiscard]] auto generate_prekey_bundle_announcement(const std::string &version) const -> std::string;

  auto assign_contact_alias(const std::string &rdx, const std::string &alias) const -> void;

  [[nodiscard]] auto create_and_sign_encrypted_message(const std::string &rdx,
    const std::string &content,
    uint32_t timestamp,
    const std::string &version) const -> std::string;

  [[nodiscard]] auto lookup_contact(const std::string &alias) const -> contact_info;

  [[nodiscard]] auto sign_nostr_event(const std::string &event_json) const -> std::string;

  [[nodiscard]] auto get_rust_bridge() -> SignalBridge &;

private:
  mutable rust::Box<SignalBridge> bridge_;
};

static_assert(concepts::signal_bridge<bridge>);

}// namespace radix_relay::signal
