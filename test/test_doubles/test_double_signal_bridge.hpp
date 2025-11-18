#pragma once

#include <concepts/signal_bridge.hpp>
#include <core/contact_info.hpp>
#include <string>
#include <vector>

namespace radix_relay_test {

struct test_double_signal_bridge
{
  mutable std::vector<std::string> called_methods;
  std::string fingerprint_to_return = "RDX:test_fingerprint";
  std::vector<radix_relay::core::contact_info> contacts_to_return;

  auto get_node_fingerprint() const -> std::string
  {
    called_methods.push_back("get_node_fingerprint");
    return fingerprint_to_return;
  }

  auto list_contacts() const -> std::vector<radix_relay::core::contact_info>
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

  auto encrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) const -> std::vector<uint8_t>
  {
    called_methods.push_back("encrypt_message");
    return bytes;
  }

  auto decrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) const -> std::vector<uint8_t>
  {
    called_methods.push_back("decrypt_message");
    return bytes;
  }

  auto add_contact_and_establish_session_from_base64(const std::string & /*bundle*/,
    const std::string & /*alias*/) const -> std::string
  {
    called_methods.push_back("add_contact_and_establish_session_from_base64");
    return "RDX:new_contact";
  }

  auto generate_prekey_bundle_announcement(const std::string & /*version*/) const -> std::string
  {
    called_methods.push_back("generate_prekey_bundle_announcement");
    return "{}";
  }

  auto extract_rdx_from_bundle_base64(const std::string & /*bundle_base64*/) const -> std::string
  {
    called_methods.push_back("extract_rdx_from_bundle_base64");
    return "RDX:extracted_fingerprint";
  }

  auto assign_contact_alias(const std::string & /*rdx*/, const std::string & /*alias*/) const -> void
  {
    called_methods.push_back("assign_contact_alias");
  }

  auto lookup_contact(const std::string & /*alias*/) const -> radix_relay::core::contact_info
  {
    called_methods.push_back("lookup_contact");
    return radix_relay::core::contact_info{
      .rdx_fingerprint = "RDX:test_contact",
      .nostr_pubkey = "npub_test",
      .user_alias = "test_alias",
      .has_active_session = true,
    };
  }

  auto create_and_sign_encrypted_message(const std::string & /*rdx*/,
    const std::string & /*content*/,
    uint32_t /*timestamp*/,
    const std::string & /*version*/) const -> std::string
  {
    called_methods.push_back("create_and_sign_encrypted_message");
    return "{}";
  }

  auto sign_nostr_event(const std::string & /*event_json*/) const -> std::string
  {
    called_methods.push_back("sign_nostr_event");
    return "{}";
  }

  auto create_subscription_for_self(const std::string & /*subscription_id*/,
    std::uint64_t /*since_timestamp*/ = 0) const -> std::string
  {
    called_methods.push_back("create_subscription_for_self");
    return R"(["REQ","sub123",{"kinds":[4],"#p":["test_pubkey"]}])";
  }

  auto update_last_message_timestamp(std::uint64_t /*timestamp*/) const -> void
  {
    called_methods.push_back("update_last_message_timestamp");
  }
};

static_assert(radix_relay::concepts::signal_bridge<test_double_signal_bridge>);

}// namespace radix_relay_test
