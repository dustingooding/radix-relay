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
  concept Command = std::same_as<T, connect> || std::same_as<T, send> || std::same_as<T, disconnect>;

  template<typename T>
  concept Event = std::same_as<T, connected> || std::same_as<T, connect_failed> || std::same_as<T, sent>
                  || std::same_as<T, send_failed> || std::same_as<T, bytes_received> || std::same_as<T, disconnected>;

  using command_variant_t = std::variant<connect, send, disconnect>;
  using event_variant_t = std::variant<connected, connect_failed, sent, send_failed, bytes_received, disconnected>;

}// namespace transport

template<typename T>
concept Command = std::same_as<T, help> || std::same_as<T, peers> || std::same_as<T, status>
                  || std::same_as<T, sessions> || std::same_as<T, scan> || std::same_as<T, version>
                  || std::same_as<T, mode> || std::same_as<T, send> || std::same_as<T, broadcast>
                  || std::same_as<T, connect> || std::same_as<T, publish_identity> || std::same_as<T, trust>
                  || std::same_as<T, verify> || std::same_as<T, subscribe> || std::same_as<T, establish_session>;

template<typename T>
concept TransportEvent = std::same_as<T, message_received> || std::same_as<T, session_established>
                         || std::same_as<T, bundle_announcement_received> || std::same_as<T, message_sent>
                         || std::same_as<T, bundle_published> || std::same_as<T, subscription_established>;

using transport_event_variant_t = std::variant<message_received,
  session_established,
  bundle_announcement_received,
  message_sent,
  bundle_published,
  subscription_established>;

template<typename T>
concept Event = Command<T> || TransportEvent<T> || std::same_as<T, raw_command>;

}// namespace radix_relay::core::events
