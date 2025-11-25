#include <signal/signal_bridge.hpp>

#include <algorithm>
#include <iterator>

namespace radix_relay::signal {

auto bridge::get_node_fingerprint() const -> std::string
{
  return std::string(radix_relay::generate_node_fingerprint(*bridge_));
}

auto bridge::list_contacts() const -> std::vector<core::contact_info>
{
  auto rust_contacts = radix_relay::list_contacts(*bridge_);
  std::vector<core::contact_info> result;
  result.reserve(rust_contacts.size());

  std::ranges::transform(rust_contacts, std::back_inserter(result), [](const auto &contact) {
    return core::contact_info{
      .rdx_fingerprint = std::string(contact.rdx_fingerprint),
      .nostr_pubkey = std::string(contact.nostr_pubkey),
      .user_alias = std::string(contact.user_alias),
      .has_active_session = contact.has_active_session,
    };
  });

  return result;
}

auto bridge::encrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const -> std::vector<uint8_t>
{
  auto encrypted =
    radix_relay::encrypt_message(*bridge_, rdx.c_str(), rust::Slice<const uint8_t>{ bytes.data(), bytes.size() });
  return { encrypted.begin(), encrypted.end() };
}

auto bridge::decrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const -> decryption_result
{
  auto result =
    radix_relay::decrypt_message(*bridge_, rdx.c_str(), rust::Slice<const uint8_t>{ bytes.data(), bytes.size() });
  return {
    .plaintext = { result.plaintext.begin(), result.plaintext.end() },
    .should_republish_bundle = result.should_republish_bundle,
  };
}

auto bridge::add_contact_and_establish_session_from_base64(const std::string &bundle, const std::string &alias) const
  -> std::string
{
  auto peer_rdx = radix_relay::add_contact_and_establish_session_from_base64(*bridge_, bundle.c_str(), alias.c_str());
  return std::string(peer_rdx);
}

auto bridge::extract_rdx_from_bundle_base64(const std::string &bundle_base64) const -> std::string
{
  auto rdx = radix_relay::extract_rdx_from_bundle_base64(*bridge_, bundle_base64.c_str());
  return std::string(rdx);
}

auto bridge::generate_prekey_bundle_announcement(const std::string &version) const -> bundle_info
{
  auto bundle_result = radix_relay::generate_prekey_bundle_announcement(*bridge_, version.c_str());
  return {
    .announcement_json = std::string(bundle_result.announcement_json),
    .pre_key_id = bundle_result.pre_key_id,
    .signed_pre_key_id = bundle_result.signed_pre_key_id,
    .kyber_pre_key_id = bundle_result.kyber_pre_key_id,
  };
}

auto bridge::generate_empty_bundle_announcement(const std::string &version) const -> std::string
{
  auto bundle_json = radix_relay::generate_empty_bundle_announcement(*bridge_, version.c_str());
  return std::string(bundle_json);
}

auto bridge::assign_contact_alias(const std::string &rdx, const std::string &alias) const -> void
{
  radix_relay::assign_contact_alias(*bridge_, rdx.c_str(), alias.c_str());
}

auto bridge::create_and_sign_encrypted_message(const std::string &rdx,
  const std::string &content,
  uint32_t timestamp,
  const std::string &version) const -> std::string
{
  auto signed_event =
    radix_relay::create_and_sign_encrypted_message(*bridge_, rdx.c_str(), content.c_str(), timestamp, version.c_str());
  return std::string(signed_event);
}

auto bridge::lookup_contact(const std::string &alias) const -> core::contact_info
{
  auto rust_contact = radix_relay::lookup_contact(*bridge_, alias.c_str());
  return {
    .rdx_fingerprint = std::string(rust_contact.rdx_fingerprint),
    .nostr_pubkey = std::string(rust_contact.nostr_pubkey),
    .user_alias = std::string(rust_contact.user_alias),
    .has_active_session = rust_contact.has_active_session,
  };
}

auto bridge::sign_nostr_event(const std::string &event_json) const -> std::string
{
  auto signed_event = radix_relay::sign_nostr_event(*bridge_, event_json.c_str());
  return std::string(signed_event);
}

auto bridge::create_subscription_for_self(const std::string &subscription_id, std::uint64_t since_timestamp) const
  -> std::string
{
  auto subscription_json =
    radix_relay::create_subscription_for_self(*bridge_, subscription_id.c_str(), since_timestamp);
  return std::string(subscription_json);
}

auto bridge::update_last_message_timestamp(std::uint64_t timestamp) const -> void
{
  radix_relay::update_last_message_timestamp(*bridge_, timestamp);
}

auto bridge::perform_key_maintenance() const -> signal::key_maintenance_result
{
  const radix_relay::KeyMaintenanceResult rust_result = radix_relay::perform_key_maintenance(*bridge_);
  return {
    .signed_pre_key_rotated = rust_result.signed_pre_key_rotated,
    .kyber_pre_key_rotated = rust_result.kyber_pre_key_rotated,
    .pre_keys_replenished = rust_result.pre_keys_replenished,
  };
}

auto bridge::record_published_bundle(std::uint32_t pre_key_id,
  std::uint32_t signed_pre_key_id,
  std::uint32_t kyber_pre_key_id) const -> void
{
  radix_relay::record_published_bundle(*bridge_, pre_key_id, signed_pre_key_id, kyber_pre_key_id);
}

}// namespace radix_relay::signal
