#pragma once

#include <algorithm>
#include <concepts/signal_bridge.hpp>
#include <core/contact_info.hpp>
#include <signal_types/signal_types.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace radix_relay_test {

struct test_double_signal_bridge
{
  mutable std::vector<std::string> called_methods;
  std::string fingerprint_to_return = "RDX:test_fingerprint";
  mutable std::vector<radix_relay::core::contact_info> contacts_to_return;
  mutable std::uint64_t last_message_timestamp = 0;

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

  auto decrypt_message(const std::string & /*rdx*/, const std::vector<uint8_t> &bytes) const
    -> radix_relay::signal::decryption_result
  {
    called_methods.push_back("decrypt_message");
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

  auto lookup_contact(const std::string &alias) const -> radix_relay::core::contact_info
  {
    called_methods.push_back("lookup_contact");
    if (not contacts_to_return.empty()) {
      const auto it = std::find_if(contacts_to_return.cbegin(),
        contacts_to_return.cend(),
        [&alias](const auto &contact) { return contact.rdx_fingerprint == alias or contact.user_alias == alias; });
      if (it != contacts_to_return.cend()) { return *it; }
      throw std::runtime_error("Contact not found: " + alias);
    }
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

  auto create_subscription_for_self(const std::string &subscription_id, std::uint64_t since_timestamp = 0) const
    -> std::string
  {
    called_methods.push_back("create_subscription_for_self");
    const auto timestamp_to_use = since_timestamp > 0 ? since_timestamp : last_message_timestamp;
    if (timestamp_to_use > 0) {
      return R"(["REQ",")" + subscription_id + R"(",{"kinds":[40001],"#p":["test_pubkey"],"since":)"
             + std::to_string(timestamp_to_use) + R"(}])";
    }
    return R"(["REQ",")" + subscription_id + R"(",{"kinds":[40001],"#p":["test_pubkey"]}])";
  }

  auto update_last_message_timestamp(std::uint64_t timestamp) const -> void
  {
    called_methods.push_back("update_last_message_timestamp");
    last_message_timestamp = timestamp;
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

  auto get_conversation_messages(const std::string &rdx_fingerprint,
    std::uint32_t limit,
    std::uint32_t /*offset*/) const -> std::vector<radix_relay::signal::stored_message>
  {
    called_methods.push_back("get_conversation_messages");
    std::vector<radix_relay::signal::stored_message> result;
    const auto target_conv_id = conversation_id_for_rdx(rdx_fingerprint);
    std::copy_if(messages_to_return.begin(),
      messages_to_return.end(),
      std::back_inserter(result),
      [target_conv_id](const auto &msg) { return msg.conversation_id == target_conv_id; });
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.timestamp > b.timestamp; });
    if (result.size() > static_cast<size_t>(limit)) { result.resize(limit); }
    return result;
  }

  auto mark_conversation_read(const std::string &rdx_fingerprint) const -> void
  {
    called_methods.push_back("mark_conversation_read");
    marked_read_rdx = rdx_fingerprint;
  }

  auto mark_conversation_read_up_to(const std::string &rdx_fingerprint, std::uint64_t up_to_timestamp) const -> void
  {
    called_methods.push_back("mark_conversation_read_up_to");
    marked_read_rdx = rdx_fingerprint;
    marked_read_up_to_timestamp = up_to_timestamp;
  }

  auto get_unread_count(const std::string & /*rdx_fingerprint*/) const -> std::uint32_t
  {
    called_methods.push_back("get_unread_count");
    return unread_count_to_return;
  }

  auto get_conversations(bool /*include_archived*/) const -> std::vector<radix_relay::signal::conversation>
  {
    called_methods.push_back("get_conversations");
    return conversations_to_return;
  }

  auto delete_message(std::int64_t /*message_id*/) const -> void { called_methods.push_back("delete_message"); }

  auto delete_conversation(const std::string & /*rdx_fingerprint*/) const -> void
  {
    called_methods.push_back("delete_conversation");
  }

  mutable std::vector<radix_relay::signal::stored_message> messages_to_return;
  mutable std::vector<radix_relay::signal::conversation> conversations_to_return;
  mutable std::uint32_t unread_count_to_return = 0;
  mutable std::string marked_read_rdx;
  mutable std::uint64_t marked_read_up_to_timestamp = 0;

private:
  radix_relay::signal::key_maintenance_result maintenance_result{
    .signed_pre_key_rotated = false,
    .kyber_pre_key_rotated = false,
    .pre_keys_replenished = false,
  };

  static auto conversation_id_for_rdx(const std::string &rdx) -> std::int64_t
  {
    return static_cast<std::int64_t>(std::hash<std::string>{}(rdx) % 1000);
  }
};

static_assert(radix_relay::concepts::signal_bridge<test_double_signal_bridge>);

}// namespace radix_relay_test
