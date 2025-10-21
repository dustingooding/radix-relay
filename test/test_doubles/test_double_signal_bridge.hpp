#pragma once

#include <radix_relay/concepts/signal_bridge.hpp>
#include <radix_relay/signal_types.hpp>
#include <string>
#include <vector>

namespace radix_relay_test {

struct TestDoubleSignalBridge
{
  mutable std::vector<std::string> called_methods;
  std::string fingerprint_to_return = "RDX:test_fingerprint";
  std::vector<radix_relay::signal::contact_info> contacts_to_return;

  auto get_node_fingerprint() -> std::string
  {
    called_methods.push_back("get_node_fingerprint");
    return fingerprint_to_return;
  }

  auto list_contacts() -> std::vector<radix_relay::signal::contact_info>
  {
    called_methods.push_back("list_contacts");
    return contacts_to_return;
  }

  auto was_called(const std::string &method) const -> bool
  {
    return std::find(called_methods.begin(), called_methods.end(), method) != called_methods.end();
  }

  auto call_count(const std::string &method) const -> size_t
  {
    return static_cast<size_t>(std::count(called_methods.begin(), called_methods.end(), method));
  }

  auto clear_calls() -> void { called_methods.clear(); }

  auto encrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) -> std::vector<uint8_t>
  {
    called_methods.push_back("encrypt_message");
    return bytes;
  }

  auto decrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) -> std::vector<uint8_t>
  {
    called_methods.push_back("decrypt_message");
    return bytes;
  }

  auto add_contact_and_establish_session_from_base64(const std::string & /*bundle*/, const std::string & /*alias*/)
    -> std::string
  {
    called_methods.push_back("add_contact_and_establish_session_from_base64");
    return "RDX:new_contact";
  }

  auto generate_prekey_bundle_announcement(const std::string & /*version*/) -> std::string
  {
    called_methods.push_back("generate_prekey_bundle_announcement");
    return "{}";
  }

  auto assign_contact_alias(const std::string & /*rdx*/, const std::string & /*alias*/) -> void
  {
    called_methods.push_back("assign_contact_alias");
  }

  auto lookup_contact(const std::string & /*alias*/) -> radix_relay::signal::contact_info
  {
    called_methods.push_back("lookup_contact");
    return radix_relay::signal::contact_info{
      .rdx_fingerprint = "RDX:test_contact",
      .nostr_pubkey = "npub_test",
      .user_alias = "test_alias",
      .has_active_session = true,
    };
  }

  auto create_and_sign_encrypted_message(const std::string & /*rdx*/,
    const std::string & /*content*/,
    uint32_t /*timestamp*/,
    const std::string & /*version*/) -> std::string
  {
    called_methods.push_back("create_and_sign_encrypted_message");
    return "{}";
  }

  auto sign_nostr_event(const std::string & /*event_json*/) -> std::string
  {
    called_methods.push_back("sign_nostr_event");
    return "{}";
  }
};

static_assert(radix_relay::concepts::signal_bridge<TestDoubleSignalBridge>);

}// namespace radix_relay_test
