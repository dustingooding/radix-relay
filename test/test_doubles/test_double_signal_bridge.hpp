#pragma once

#include <algorithm>
#include <concepts/signal_bridge.hpp>
#include <core/contact_info.hpp>
#include <core/signal_types.hpp>
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
    return std::any_of(called_methods.cbegin(), called_methods.cend(), [&method](const std::string &called) {
      return called == method;
    });
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

  auto decrypt_message_with_metadata(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) const
    -> radix_relay::signal::decryption_result
  {
    called_methods.push_back("decrypt_message_with_metadata");
    return {
      .plaintext = bytes,
      .should_republish_bundle = false,
    };
  }

  auto add_contact_and_establish_session_from_base64(const std::string & /*bundle*/,
    const std::string & /*alias*/) const -> std::string
  {
    called_methods.push_back("add_contact_and_establish_session_from_base64");
    return "RDX:new_contact";
  }

  auto generate_prekey_bundle_announcement(const std::string & /*version*/) const -> radix_relay::signal::bundle_info
  {
    called_methods.push_back("generate_prekey_bundle_announcement");
    return {
      .announcement_json = R"({
        "id": "test_bundle_event_id",
        "pubkey": "test_pubkey",
        "created_at": 1234567890,
        "kind": 30078,
        "tags": [["d", "radix_prekey_bundle_v1"], ["v", "test-0.1.0"]],
        "content": "test_bundle_content_base64",
        "sig": "test_signature"
      })",
      .pre_key_id = 100,
      .signed_pre_key_id = 1,
      .kyber_pre_key_id = 1,
    };
  }

  auto generate_empty_bundle_announcement(const std::string & /*version*/) const -> std::string
  {
    called_methods.push_back("generate_empty_bundle_announcement");
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

  auto perform_key_maintenance() const -> radix_relay::signal::key_maintenance_result
  {
    called_methods.push_back("perform_key_maintenance");
    return maintenance_result;
  }

  auto record_published_bundle(std::uint32_t /*pre_key_id*/,
    std::uint32_t /*signed_pre_key_id*/,
    std::uint32_t /*kyber_pre_key_id*/) const -> void
  {
    called_methods.push_back("record_published_bundle");
  }

  auto set_maintenance_result(radix_relay::signal::key_maintenance_result result) -> void
  {
    maintenance_result = result;
  }

private:
  radix_relay::signal::key_maintenance_result maintenance_result{
    .signed_pre_key_rotated = false,
    .kyber_pre_key_rotated = false,
    .pre_keys_replenished = false,
  };
};

static_assert(radix_relay::concepts::signal_bridge<test_double_signal_bridge>);

}// namespace radix_relay_test
