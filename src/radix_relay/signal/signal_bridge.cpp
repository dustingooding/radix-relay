#include <radix_relay/signal/signal_bridge.hpp>

#include <algorithm>
#include <iterator>

namespace radix_relay::signal {

auto bridge::get_node_fingerprint() const -> std::string { return radix_relay::signal::get_node_fingerprint(*bridge_); }

auto bridge::list_contacts() const -> std::vector<signal::contact_info>
{
  auto rust_contacts = radix_relay::list_contacts(*bridge_);
  std::vector<signal::contact_info> result;
  result.reserve(rust_contacts.size());

  std::ranges::transform(rust_contacts, std::back_inserter(result), [](const auto &contact) {
    return signal::contact_info{
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

auto bridge::decrypt_message(const std::string &rdx, const std::vector<uint8_t> &bytes) const -> std::vector<uint8_t>
{
  auto decrypted =
    radix_relay::decrypt_message(*bridge_, rdx.c_str(), rust::Slice<const uint8_t>{ bytes.data(), bytes.size() });
  return { decrypted.begin(), decrypted.end() };
}

auto bridge::add_contact_and_establish_session_from_base64(const std::string &bundle, const std::string &alias) const
  -> std::string
{
  auto peer_rdx = radix_relay::add_contact_and_establish_session_from_base64(*bridge_, bundle.c_str(), alias.c_str());
  return std::string(peer_rdx);
}

auto bridge::generate_prekey_bundle_announcement(const std::string &version) const -> std::string
{
  auto bundle_json = radix_relay::generate_prekey_bundle_announcement(*bridge_, version.c_str());
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

auto bridge::lookup_contact(const std::string &alias) const -> signal::contact_info
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

auto bridge::create_subscription_for_self(const std::string &subscription_id) const -> std::string
{
  auto subscription_json = radix_relay::create_subscription_for_self(*bridge_, subscription_id.c_str());
  return std::string(subscription_json);
}

}// namespace radix_relay::signal
