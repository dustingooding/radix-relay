#include "internal_use_only/config.hpp"
#include <radix_relay/nostr/protocol.hpp>

#include <algorithm>
#include <bit>
#include <iterator>

namespace radix_relay::nostr::protocol {

auto event_data::deserialize(std::span<const std::byte> bytes) -> std::optional<event_data>
{
  std::string json_str;
  json_str.resize(bytes.size());
  std::ranges::transform(bytes, json_str.begin(), [](std::byte byte_val) { return std::bit_cast<char>(byte_val); });
  return deserialize(json_str);
}

auto event_data::deserialize(const std::string &json) -> std::optional<event_data>
{
  try {
    auto json_obj = nlohmann::json::parse(json);

    event_data event;

    if (!json_obj.contains("id") || !json_obj["id"].is_string()) { return std::nullopt; }
    event.id = json_obj["id"].get<std::string>();

    if (!json_obj.contains("pubkey") || !json_obj["pubkey"].is_string()) { return std::nullopt; }
    event.pubkey = json_obj["pubkey"].get<std::string>();

    if (!json_obj.contains("created_at") || !json_obj["created_at"].is_number()) { return std::nullopt; }
    event.created_at = json_obj["created_at"].get<std::uint64_t>();

    if (!json_obj.contains("kind") || !json_obj["kind"].is_number()) { return std::nullopt; }
    event.kind = json_obj["kind"].get<enum kind>();

    if (!json_obj.contains("content") || !json_obj["content"].is_string()) { return std::nullopt; }
    event.content = json_obj["content"].get<std::string>();

    if (!json_obj.contains("sig") || !json_obj["sig"].is_string()) { return std::nullopt; }
    event.sig = json_obj["sig"].get<std::string>();

    if (json_obj.contains("tags")) {
      if (!json_obj["tags"].is_array()) { return std::nullopt; }
      for (const auto &tag_json : json_obj["tags"]) {
        if (!tag_json.is_array()) { return std::nullopt; }
        std::vector<std::string> tag;
        for (const auto &element : tag_json) {
          if (!element.is_string()) { return std::nullopt; }
          tag.push_back(element.get<std::string>());
        }
        event.tags.push_back(std::move(tag));
      }
    }

    return event;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

auto event_data::serialize() const -> std::vector<std::byte>
{
  nlohmann::json json_obj;

  json_obj["id"] = id;
  json_obj["pubkey"] = pubkey;
  json_obj["created_at"] = created_at;
  json_obj["kind"] = kind;
  json_obj["content"] = content;
  json_obj["sig"] = sig;

  json_obj["tags"] = nlohmann::json::array();
  for (const auto &tag : tags) {
    nlohmann::json tag_json = nlohmann::json::array();
    std::ranges::copy(tag, std::back_inserter(tag_json));
    json_obj["tags"].push_back(tag_json);
  }

  auto json_str = json_obj.dump();
  std::vector<std::byte> bytes;
  bytes.resize(json_str.size());
  std::ranges::transform(json_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
  return bytes;
}

auto event_data::create_identity_announcement(const std::string &sender_pubkey,
  std::uint64_t timestamp,
  const std::string &signal_fingerprint,
  const std::string &capabilities) -> event_data
{
  return { .id = "",
    .pubkey = sender_pubkey,
    .created_at = timestamp,
    .kind = kind::identity_announcement,
    .tags = { { "signal_fingerprint", signal_fingerprint },
      { "radix_capabilities", capabilities },
      { "radix_version", std::string{ radix_relay::cmake::project_version } } },
    .content = "radix_relay_node_v1",
    .sig = "" };
}

auto event_data::create_bundle_announcement(const std::string &sender_pubkey,
  std::uint64_t timestamp,
  const std::string &bundle_hex) -> event_data
{
  return { .id = "",
    .pubkey = sender_pubkey,
    .created_at = timestamp,
    .kind = kind::bundle_announcement,
    .tags = { { "radix_version", std::string{ radix_relay::cmake::project_version } } },
    .content = bundle_hex,
    .sig = "" };
}

auto event_data::create_encrypted_message(std::uint64_t timestamp,
  const std::string &recipient_pubkey,
  const std::string &encrypted_payload,
  const std::string &session_id) -> event_data
{
  return { .id = "",
    .pubkey = "",
    .created_at = timestamp,
    .kind = kind::encrypted_message,
    .tags = { { "p", recipient_pubkey },
      { "radix_peer", session_id },
      { "radix_version", std::string{ radix_relay::cmake::project_version } } },
    .content = encrypted_payload,
    .sig = "" };
}

auto event_data::create_session_request(const std::string &sender_pubkey,
  std::uint64_t timestamp,
  const std::string &recipient_pubkey,
  const std::string &prekey_bundle) -> event_data
{
  return { .id = "",
    .pubkey = sender_pubkey,
    .created_at = timestamp,
    .kind = kind::session_request,
    .tags = { { "p", recipient_pubkey }, { "radix_version", std::string{ radix_relay::cmake::project_version } } },
    .content = prekey_bundle,
    .sig = "" };
}

auto event_data::is_radix_message() const -> bool
{
  switch (kind) {
  case kind::encrypted_message:
  case kind::identity_announcement:
  case kind::session_request:
  case kind::node_status:
  case kind::bundle_announcement:
    return true;
  case kind::profile_metadata:
  case kind::text_note:
  case kind::recommend_relay:
  case kind::contact_list:
  case kind::encrypted_dm:
  case kind::reaction:
  case kind::parameterized_replaceable_start:
    return false;
  }
  return false;
}

auto event_data::get_kind() const -> std::optional<enum kind>
{
  switch (kind) {
  case kind::profile_metadata:
  case kind::text_note:
  case kind::recommend_relay:
  case kind::contact_list:
  case kind::encrypted_dm:
  case kind::reaction:
  case kind::bundle_announcement:
  case kind::encrypted_message:
  case kind::identity_announcement:
  case kind::session_request:
  case kind::node_status:
  case kind::parameterized_replaceable_start:
    return kind;
  }
  return std::nullopt;
}

auto ok::deserialize(const std::string &json) -> std::optional<ok>
{
  try {
    auto json_obj = nlohmann::json::parse(json);

    if (!json_obj.is_array() || json_obj.size() < 3) { return std::nullopt; }
    if (!json_obj[0].is_string() || json_obj[0].get<std::string>() != "OK") { return std::nullopt; }
    if (!json_obj[1].is_string()) { return std::nullopt; }
    if (!json_obj[2].is_boolean()) { return std::nullopt; }

    ok result;
    result.event_id = json_obj[1].get<std::string>();
    result.accepted = json_obj[2].get<bool>();
    result.message = (json_obj.size() > 3 && json_obj[3].is_string()) ? json_obj[3].get<std::string>() : "";

    return result;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

auto eose::deserialize(const std::string &json) -> std::optional<eose>
{
  try {
    auto json_obj = nlohmann::json::parse(json);

    if (!json_obj.is_array() || json_obj.size() < 2) { return std::nullopt; }
    if (!json_obj[0].is_string() || json_obj[0].get<std::string>() != "EOSE") { return std::nullopt; }
    if (!json_obj[1].is_string()) { return std::nullopt; }

    eose result;
    result.subscription_id = json_obj[1].get<std::string>();

    return result;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

auto req::serialize() const -> std::string
{
  nlohmann::json json_array = nlohmann::json::array();
  json_array.push_back("REQ");
  json_array.push_back(subscription_id);
  json_array.push_back(filters);
  return json_array.dump();
}

auto req::deserialize(const std::string &json) -> std::optional<req>
{
  try {
    auto json_obj = nlohmann::json::parse(json);

    if (!json_obj.is_array() || json_obj.size() < 3) { return std::nullopt; }
    if (!json_obj[0].is_string() || json_obj[0].get<std::string>() != "REQ") { return std::nullopt; }
    if (!json_obj[1].is_string()) { return std::nullopt; }
    if (!json_obj[2].is_object()) { return std::nullopt; }

    req result;
    result.subscription_id = json_obj[1].get<std::string>();
    result.filters = json_obj[2];

    return result;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

auto event::from_event_data(const event_data &evt) -> event { return event{ .subscription_id = "", .data = evt }; }

auto event::serialize() const -> std::string
{
  nlohmann::json event_json;
  event_json["id"] = data.id;
  event_json["pubkey"] = data.pubkey;
  event_json["created_at"] = data.created_at;
  event_json["kind"] = data.kind;
  event_json["content"] = data.content;
  event_json["sig"] = data.sig;
  event_json["tags"] = nlohmann::json::array();
  for (const auto &tag : data.tags) {
    nlohmann::json tag_json = nlohmann::json::array();
    std::ranges::copy(tag, std::back_inserter(tag_json));
    event_json["tags"].push_back(tag_json);
  }

  nlohmann::json message = nlohmann::json::array();
  message.push_back("EVENT");
  if (!subscription_id.empty()) { message.push_back(subscription_id); }
  message.push_back(event_json);

  return message.dump();
}

auto event::deserialize(const std::string &json) -> std::optional<event>
{
  try {
    auto json_obj = nlohmann::json::parse(json);

    if (!json_obj.is_array() || json_obj.empty()) { return std::nullopt; }
    if (!json_obj[0].is_string() || json_obj[0].get<std::string>() != "EVENT") { return std::nullopt; }

    event result;

    if (json_obj.size() == 2) {
      if (!json_obj[1].is_object()) { return std::nullopt; }

      auto event_opt = event_data::deserialize(json_obj[1].dump());
      if (!event_opt) { return std::nullopt; }

      result.data = std::move(*event_opt);
    }
    if (json_obj.size() == 3) {
      if (!json_obj[1].is_string()) { return std::nullopt; }
      if (!json_obj[2].is_object()) { return std::nullopt; }

      auto event_opt = event_data::deserialize(json_obj[2].dump());
      if (!event_opt) { return std::nullopt; }

      result.subscription_id = json_obj[1].get<std::string>();
      result.data = std::move(*event_opt);
    }
    return result;
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

}// namespace radix_relay::nostr::protocol
