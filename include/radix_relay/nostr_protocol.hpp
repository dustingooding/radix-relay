#pragma once

#include "internal_use_only/config.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace radix_relay::nostr::protocol {

enum class kind : std::uint32_t {
  // Standard Nostr kinds
  profile_metadata = 0,
  text_note = 1,
  recommend_relay = 2,
  contact_list = 3,
  encrypted_dm = 4,
  reaction = 7,

  // Radix Relay custom kinds (40001-40099 reserved)
  encrypted_message = 40001,
  identity_announcement = 40002,
  session_request = 40003,
  node_status = 40004,
};

struct event_data
{
  std::string id;
  std::string pubkey;
  std::uint64_t created_at;
  std::uint32_t kind;
  std::vector<std::vector<std::string>> tags;
  std::string content;
  std::string sig;

  static auto deserialize(std::span<const std::byte> bytes) -> std::optional<event_data>
  {
    std::string json_str;
    json_str.resize(bytes.size());
    std::ranges::transform(bytes, json_str.begin(), [](std::byte b) { return std::bit_cast<char>(b); });
    return deserialize(json_str);
  }

  static auto deserialize(const std::string &json) -> std::optional<event_data>
  {
    try {
      auto j = nlohmann::json::parse(json);

      event_data event;

      // Required fields
      if (!j.contains("id") || !j["id"].is_string()) return std::nullopt;
      event.id = j["id"].get<std::string>();

      if (!j.contains("pubkey") || !j["pubkey"].is_string()) return std::nullopt;
      event.pubkey = j["pubkey"].get<std::string>();

      if (!j.contains("created_at") || !j["created_at"].is_number()) return std::nullopt;
      event.created_at = j["created_at"].get<std::uint64_t>();

      if (!j.contains("kind") || !j["kind"].is_number()) return std::nullopt;
      event.kind = j["kind"].get<std::uint32_t>();

      if (!j.contains("content") || !j["content"].is_string()) return std::nullopt;
      event.content = j["content"].get<std::string>();

      if (!j.contains("sig") || !j["sig"].is_string()) return std::nullopt;
      event.sig = j["sig"].get<std::string>();

      // Tags field (optional)
      if (j.contains("tags")) {
        if (!j["tags"].is_array()) return std::nullopt;
        for (const auto &tag_json : j["tags"]) {
          if (!tag_json.is_array()) return std::nullopt;
          std::vector<std::string> tag;
          for (const auto &element : tag_json) {
            if (!element.is_string()) return std::nullopt;
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

  auto serialize() const -> std::vector<std::byte>
  {
    nlohmann::json j;

    j["id"] = id;
    j["pubkey"] = pubkey;
    j["created_at"] = created_at;
    j["kind"] = kind;
    j["content"] = content;
    j["sig"] = sig;

    j["tags"] = nlohmann::json::array();
    for (const auto &tag : tags) {
      nlohmann::json tag_json = nlohmann::json::array();
      std::copy(tag.begin(), tag.end(), std::back_inserter(tag_json));
      j["tags"].push_back(tag_json);
    }

    auto json_str = j.dump();
    std::vector<std::byte> bytes;
    bytes.resize(json_str.size());
    std::ranges::transform(json_str, bytes.begin(), [](char c) { return std::bit_cast<std::byte>(c); });
    return bytes;
  }

  static auto create_identity_announcement(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &signal_fingerprint,
    const std::string &capabilities = "mesh,nostr") -> event_data
  {
    return {
      .id = "",// Will be computed when signing
      .pubkey = sender_pubkey,
      .created_at = timestamp,
      .kind = static_cast<std::uint32_t>(kind::identity_announcement),
      .tags = { { "signal_fingerprint", signal_fingerprint },
        { "radix_capabilities", capabilities },
        { "radix_version", std::string{ radix_relay::cmake::project_version } } },
      .content = "radix_relay_node_v1",
      .sig = ""// Will be computed when signing
    };
  }

  static auto create_encrypted_message(std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &encrypted_payload,
    const std::string &session_id) -> event_data
  {
    return {
      .id = "",// Will be computed when signing
      .pubkey = "",// Will be filled when signing
      .created_at = timestamp,
      .kind = static_cast<std::uint32_t>(kind::encrypted_message),
      .tags = { { "p", recipient_pubkey },
        { "radix_peer", session_id },
        { "radix_version", std::string{ radix_relay::cmake::project_version } } },
      .content = encrypted_payload,
      .sig = ""// Will be computed when signing
    };
  }

  static auto create_session_request(const std::string &sender_pubkey,
    std::uint64_t timestamp,
    const std::string &recipient_pubkey,
    const std::string &prekey_bundle) -> event_data
  {
    return {
      .id = "",// Will be computed when signing
      .pubkey = sender_pubkey,
      .created_at = timestamp,
      .kind = static_cast<std::uint32_t>(kind::session_request),
      .tags = { { "p", recipient_pubkey }, { "radix_version", std::string{ radix_relay::cmake::project_version } } },
      .content = prekey_bundle,
      .sig = ""// Will be computed when signing
    };
  }

  auto is_radix_message() const -> bool
  {
    switch (kind) {
    case static_cast<std::uint32_t>(kind::encrypted_message):
    case static_cast<std::uint32_t>(kind::identity_announcement):
    case static_cast<std::uint32_t>(kind::session_request):
    case static_cast<std::uint32_t>(kind::node_status):
      return true;
    case static_cast<std::uint32_t>(kind::profile_metadata):
    case static_cast<std::uint32_t>(kind::text_note):
    case static_cast<std::uint32_t>(kind::recommend_relay):
    case static_cast<std::uint32_t>(kind::contact_list):
    case static_cast<std::uint32_t>(kind::encrypted_dm):
    case static_cast<std::uint32_t>(kind::reaction):
      return false;
    }
    return false;// Unknown kind
  }

  auto get_kind() const -> std::optional<enum kind>
  {
    switch (kind) {
    case 0:
      return kind::profile_metadata;
    case 1:
      return kind::text_note;
    case 2:
      return kind::recommend_relay;
    case 3:
      return kind::contact_list;
    case 4:
      return kind::encrypted_dm;
    case 7:
      return kind::reaction;
    case 40001:
      return kind::encrypted_message;
    case 40002:
      return kind::identity_announcement;
    case 40003:
      return kind::session_request;
    case 40004:
      return kind::node_status;
    default:
      return std::nullopt;
    }
  }
};

struct ok
{
  std::string event_id;
  bool accepted;
  std::string message;

  static auto deserialize(const std::string &json) -> std::optional<ok>
  {
    try {
      auto j = nlohmann::json::parse(json);

      if (!j.is_array() || j.size() < 3) return std::nullopt;
      if (!j[0].is_string() || j[0].get<std::string>() != "OK") return std::nullopt;
      if (!j[1].is_string()) return std::nullopt;
      if (!j[2].is_boolean()) return std::nullopt;

      ok result;
      result.event_id = j[1].get<std::string>();
      result.accepted = j[2].get<bool>();
      result.message = (j.size() > 3 && j[3].is_string()) ? j[3].get<std::string>() : "";

      return result;
    } catch (const std::exception &) {
      return std::nullopt;
    }
  }
};

struct eose
{
  std::string subscription_id;

  static auto deserialize(const std::string &json) -> std::optional<eose>
  {
    try {
      auto j = nlohmann::json::parse(json);

      if (!j.is_array() || j.size() < 2) return std::nullopt;
      if (!j[0].is_string() || j[0].get<std::string>() != "EOSE") return std::nullopt;
      if (!j[1].is_string()) return std::nullopt;

      eose result;
      result.subscription_id = j[1].get<std::string>();

      return result;
    } catch (const std::exception &) {
      return std::nullopt;
    }
  }
};

struct req
{
  std::string subscription_id;
  nlohmann::json filters;

  auto serialize() const -> std::string
  {
    nlohmann::json j = nlohmann::json::array();
    j.push_back("REQ");
    j.push_back(subscription_id);
    j.push_back(filters);
    return j.dump();
  }

  static auto deserialize(const std::string &json) -> std::optional<req>
  {
    try {
      auto j = nlohmann::json::parse(json);

      if (!j.is_array() || j.size() < 3) return std::nullopt;
      if (!j[0].is_string() || j[0].get<std::string>() != "REQ") return std::nullopt;
      if (!j[1].is_string()) return std::nullopt;
      if (!j[2].is_object()) return std::nullopt;

      req result;
      result.subscription_id = j[1].get<std::string>();
      result.filters = j[2];

      return result;
    } catch (const std::exception &) {
      return std::nullopt;
    }
  }
};

struct event
{
  std::string subscription_id;
  event_data data;

  static auto from_event_data(const event_data &evt) -> event { return event{ "", evt }; }

  auto serialize() const -> std::string
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
      std::copy(tag.begin(), tag.end(), std::back_inserter(tag_json));
      event_json["tags"].push_back(tag_json);
    }

    nlohmann::json message = nlohmann::json::array();
    message.push_back("EVENT");
    // Only include subscription_id for incoming events (relay -> client)
    // Outgoing events (client -> relay) should not include it
    if (!subscription_id.empty()) { message.push_back(subscription_id); }
    message.push_back(event_json);

    return message.dump();
  }

  static auto deserialize(const std::string &json) -> std::optional<event>
  {
    try {
      auto j = nlohmann::json::parse(json);

      if (!j.is_array() || j.size() < 1) return std::nullopt;
      if (!j[0].is_string() || j[0].get<std::string>() != "EVENT") return std::nullopt;

      event result;

      if (j.size() == 2) {
        if (!j[1].is_object()) return std::nullopt;

        auto event_opt = event_data::deserialize(j[1].dump());
        if (!event_opt) return std::nullopt;

        result.data = std::move(*event_opt);
      }
      if (j.size() == 3) {
        if (!j[1].is_string()) return std::nullopt;
        if (!j[2].is_object()) return std::nullopt;

        auto event_opt = event_data::deserialize(j[2].dump());
        if (!event_opt) return std::nullopt;

        result.subscription_id = j[1].get<std::string>();
        result.data = std::move(*event_opt);
      }
      return result;
    } catch (const std::exception &) {
      return std::nullopt;
    }
  }
};

}// namespace radix_relay::nostr::protocol
