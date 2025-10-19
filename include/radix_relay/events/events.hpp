#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <variant>

namespace radix_relay::events {

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

struct publish_identity
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

template<typename T>
concept Command =
  std::same_as<T, help> || std::same_as<T, peers> || std::same_as<T, status> || std::same_as<T, sessions>
  || std::same_as<T, scan> || std::same_as<T, version> || std::same_as<T, mode> || std::same_as<T, send>
  || std::same_as<T, broadcast> || std::same_as<T, connect> || std::same_as<T, publish_identity>
  || std::same_as<T, trust> || std::same_as<T, verify> || std::same_as<T, subscribe>;

template<typename T>
concept TransportEvent =
  std::same_as<T, message_received> || std::same_as<T, session_established> || std::same_as<T, message_sent>
  || std::same_as<T, bundle_published> || std::same_as<T, subscription_established>;

using transport_event_variant_t =
  std::variant<message_received, session_established, message_sent, bundle_published, subscription_established>;

template<typename T>
concept Event = Command<T> || TransportEvent<T> || std::same_as<T, raw_command>;

}// namespace radix_relay::events
