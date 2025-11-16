#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace radix_relay::core::events {

struct help
{
};
struct peers
{
};
struct status
{
};
struct sessions
{
};
struct identities
{
};
struct scan
{
};
struct version
{
};

struct mode
{
  std::string new_mode;
};

struct send
{
  std::string peer;
  std::string message;
};

struct broadcast
{
  std::string message;
};

struct connect
{
  std::string relay;
};

struct disconnect
{
};

struct publish_identity
{
};

struct unpublish_identity
{
};

struct trust
{
  std::string peer;
  std::string alias;
};

struct verify
{
  std::string peer;
};

struct subscribe
{
  std::string subscription_json;
};

struct subscribe_identities
{
};

struct subscribe_messages
{
};

struct establish_session
{
  std::string bundle_data;
};

struct raw_command
{
  std::string input;
};

struct message_received
{
  std::string sender_rdx;
  std::string content;
  std::uint64_t timestamp;
};

struct session_established
{
  std::string peer_rdx;
};

struct bundle_announcement_received
{
  std::string pubkey;
  std::string bundle_content;
  std::string event_id;
};

struct bundle_announcement_removed
{
  std::string pubkey;
  std::string event_id;
};

struct discovered_identity
{
  std::string rdx_fingerprint;
  std::string nostr_pubkey;
  std::string event_id;
};

struct list_identities
{
};

struct identities_listed
{
  std::vector<discovered_identity> identities;
};

struct message_sent
{
  std::string peer;
  std::string event_id;
  bool accepted;
};

struct bundle_published
{
  std::string event_id;
  bool accepted;
};

struct subscription_established
{
  std::string subscription_id;
};

namespace transport {

  struct connect
  {
    std::string url;
  };

  struct connected
  {
    std::string url;
  };

  struct connect_failed
  {
    std::string url;
    std::string error_message;
  };

  struct send
  {
    std::string message_id;
    std::vector<std::byte> bytes;
  };

  struct sent
  {
    std::string message_id;
  };

  struct send_failed
  {
    std::string message_id;
    std::string error_message;
  };

  struct bytes_received
  {
    std::vector<std::byte> bytes;
  };

  struct disconnect
  {
  };

  struct disconnected
  {
  };

  template<typename T>
  concept Command = std::same_as<T, connect> or std::same_as<T, send> or std::same_as<T, disconnect>;

  template<typename T>
  concept Event = std::same_as<T, connected> or std::same_as<T, connect_failed> or std::same_as<T, sent>
                  or std::same_as<T, send_failed> or std::same_as<T, bytes_received> or std::same_as<T, disconnected>;

  using in_t = std::variant<connect, send, disconnect>;

}// namespace transport

template<typename T>
concept Command =
  std::same_as<T, help> or std::same_as<T, peers> or std::same_as<T, status> or std::same_as<T, sessions>
  or std::same_as<T, identities> or std::same_as<T, scan> or std::same_as<T, version> or std::same_as<T, mode>
  or std::same_as<T, send> or std::same_as<T, broadcast> or std::same_as<T, connect> or std::same_as<T, disconnect>
  or std::same_as<T, publish_identity> or std::same_as<T, unpublish_identity> or std::same_as<T, trust>
  or std::same_as<T, verify> or std::same_as<T, subscribe> or std::same_as<T, subscribe_identities>
  or std::same_as<T, subscribe_messages> or std::same_as<T, establish_session>;

template<typename T>
concept PresentationEvent =
  std::same_as<T, message_received> or std::same_as<T, session_established>
  or std::same_as<T, bundle_announcement_received> or std::same_as<T, bundle_announcement_removed>
  or std::same_as<T, message_sent> or std::same_as<T, bundle_published> or std::same_as<T, subscription_established>
  or std::same_as<T, identities_listed>;

using presentation_event_variant_t = std::variant<message_received,
  session_established,
  bundle_announcement_received,
  bundle_announcement_removed,
  message_sent,
  bundle_published,
  subscription_established,
  identities_listed>;

template<typename T>
concept Event = Command<T> or PresentationEvent<T> or std::same_as<T, raw_command>;

namespace session_orchestrator {

  using command_from_main_variant_t = std::variant<send,
    publish_identity,
    unpublish_identity,
    trust,
    subscribe,
    subscribe_identities,
    subscribe_messages,
    list_identities>;

  using in_t = std::variant<send,
    publish_identity,
    unpublish_identity,
    trust,
    subscribe,
    subscribe_identities,
    subscribe_messages,
    list_identities,
    connect,
    transport::bytes_received,
    transport::connected,
    transport::connect_failed,
    transport::sent,
    transport::send_failed,
    transport::disconnected,
    bundle_announcement_received,
    bundle_announcement_removed>;

}// namespace session_orchestrator

struct display_message
{
  std::string message;
};

}// namespace radix_relay::core::events
