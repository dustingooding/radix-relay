#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace radix_relay::core::events {

/// Request display of available commands
struct help
{
};

/// Request list of connected peers
struct peers
{
};

/// Request current system status
struct status
{
};

/// Request list of active sessions
struct sessions
{
};

/// Request list of discovered identities
struct identities
{
};

/// Request scan for nearby peers
struct scan
{
};

/// Request application version information
struct version
{
};

/// Change operational mode
struct mode
{
  std::string new_mode;///< The new mode to switch to
};

/// Send encrypted message to a specific peer
struct send
{
  std::string peer;///< RDX fingerprint or alias of recipient
  std::string message;///< Message content to send
};

/// Broadcast message to all peers
struct broadcast
{
  std::string message;///< Message content to broadcast
};

/// Connect to a relay
struct connect
{
  std::string relay;///< Relay URL to connect to
};

/// Disconnect from current relay
struct disconnect
{
};

/// Publish identity bundle to the network
struct publish_identity
{
};

/// Remove identity bundle from the network
struct unpublish_identity
{
};

/// Establish trust with a peer and assign an alias
struct trust
{
  std::string peer;///< RDX fingerprint of peer to trust
  std::string alias;///< Friendly alias to assign
};

/// Verify identity fingerprint of a peer
struct verify
{
  std::string peer;///< RDX fingerprint to verify
};

/// Subscribe to custom Nostr events
struct subscribe
{
  std::string subscription_json;///< JSON subscription filter
};

/// Subscribe to identity announcements
struct subscribe_identities
{
};

/// Subscribe to encrypted messages for this node
struct subscribe_messages
{
};

/// Establish session from a received bundle
struct establish_session
{
  std::string bundle_data;///< Base64-encoded prekey bundle
};

/// Raw unparsed command input
struct raw_command
{
  std::string input;///< Raw command string
};

/// Notification of received encrypted message
struct message_received
{
  std::string sender_rdx;///< RDX fingerprint of sender
  std::string sender_alias;///< Alias of sender (if known)
  std::string content;///< Decrypted message content
  std::uint64_t timestamp;///< Message timestamp
  bool should_republish_bundle;///< Whether to republish prekey bundle
};

/// Notification of successfully established session
struct session_established
{
  std::string peer_rdx;///< RDX fingerprint of peer
};

/// Notification of received bundle announcement
struct bundle_announcement_received
{
  std::string pubkey;///< Nostr public key
  std::string bundle_content;///< Bundle data
  std::string event_id;///< Nostr event ID
};

/// Notification of removed bundle announcement
struct bundle_announcement_removed
{
  std::string pubkey;///< Nostr public key
  std::string event_id;///< Nostr event ID
};

/// Discovered identity information
struct discovered_identity
{
  std::string rdx_fingerprint;///< Signal Protocol RDX fingerprint
  std::string nostr_pubkey;///< Nostr public key
  std::string event_id;///< Nostr event ID
};

/// Request list of all discovered identities
struct list_identities
{
};

/// Response containing discovered identities
struct identities_listed
{
  std::vector<discovered_identity> identities;///< List of discovered identities
};

/// Notification of sent message status
struct message_sent
{
  std::string peer;///< RDX fingerprint of recipient
  std::string event_id;///< Nostr event ID
  bool accepted;///< Whether relay accepted the message
};

/// Notification of published bundle status
struct bundle_published
{
  std::string event_id;///< Nostr event ID
  bool accepted;///< Whether relay accepted the bundle
};

/// Notification of established subscription
struct subscription_established
{
  std::string subscription_id;///< Subscription identifier
};

/// Transport type discriminator
enum class transport_type { internet, bluetooth };

/// Transport layer events and commands
namespace transport {

  /// Command to connect to a transport endpoint
  struct connect
  {
    std::string url;///< Transport endpoint URL
  };

  /// Notification of successful connection
  struct connected
  {
    std::string url;///< Connected transport endpoint URL
    transport_type type;///< Type of transport
  };

  /// Notification of failed connection attempt
  struct connect_failed
  {
    std::string url;///< Transport endpoint URL
    std::string error_message;///< Failure reason
    transport_type type;///< Type of transport
  };

  /// Command to send data through transport
  struct send
  {
    std::string message_id;///< Unique message identifier
    std::vector<std::byte> bytes;///< Raw data to send
  };

  /// Notification of successful send
  struct sent
  {
    std::string message_id;///< Message identifier
    transport_type type;///< Type of transport
  };

  /// Notification of failed send attempt
  struct send_failed
  {
    std::string message_id;///< Message identifier
    std::string error_message;///< Failure reason
    transport_type type;///< Type of transport
  };

  /// Notification of received data from transport
  struct bytes_received
  {
    std::vector<std::byte> bytes;///< Received raw data
  };

  /// Command to disconnect from transport
  struct disconnect
  {
  };

  /// Notification of disconnection
  struct disconnected
  {
    transport_type type;///< Type of transport
  };

  /// Concept for transport command types
  template<typename T>
  concept Command = std::same_as<T, connect> or std::same_as<T, send> or std::same_as<T, disconnect>;

  /// Concept for transport event types
  template<typename T>
  concept Event = std::same_as<T, connected> or std::same_as<T, connect_failed> or std::same_as<T, sent>
                  or std::same_as<T, send_failed> or std::same_as<T, bytes_received> or std::same_as<T, disconnected>;

  /// Variant type for transport input events
  using in_t = std::variant<connect, send, disconnect>;

}// namespace transport

namespace connection_monitor {
  /// Request current connection status
  struct query_status
  {
  };

  /// Variant type for connection monitor input events
  using in_t = std::variant<transport::connected,
    transport::connect_failed,
    transport::disconnected,
    transport::send_failed,
    query_status>;

}// namespace connection_monitor

/// Concept for user command event types
template<typename T>
concept Command =
  std::same_as<T, help> or std::same_as<T, peers> or std::same_as<T, status> or std::same_as<T, sessions>
  or std::same_as<T, identities> or std::same_as<T, scan> or std::same_as<T, version> or std::same_as<T, mode>
  or std::same_as<T, send> or std::same_as<T, broadcast> or std::same_as<T, connect> or std::same_as<T, disconnect>
  or std::same_as<T, publish_identity> or std::same_as<T, unpublish_identity> or std::same_as<T, trust>
  or std::same_as<T, verify> or std::same_as<T, subscribe> or std::same_as<T, subscribe_identities>
  or std::same_as<T, subscribe_messages> or std::same_as<T, establish_session>;

/// Concept for presentation layer event types
template<typename T>
concept PresentationEvent =
  std::same_as<T, message_received> or std::same_as<T, session_established>
  or std::same_as<T, bundle_announcement_received> or std::same_as<T, bundle_announcement_removed>
  or std::same_as<T, message_sent> or std::same_as<T, bundle_published> or std::same_as<T, subscription_established>
  or std::same_as<T, identities_listed>;

/// Variant type for presentation events
using presentation_event_variant_t = std::variant<message_received,
  session_established,
  bundle_announcement_received,
  bundle_announcement_removed,
  message_sent,
  bundle_published,
  subscription_established,
  identities_listed>;

/// Concept for all event types
template<typename T>
concept Event = Command<T> or PresentationEvent<T> or std::same_as<T, raw_command>;

/// Session orchestrator event types
namespace session_orchestrator {

  /// Variant of commands from main to session orchestrator
  using command_from_main_variant_t = std::variant<send,
    publish_identity,
    unpublish_identity,
    trust,
    subscribe,
    subscribe_identities,
    subscribe_messages,
    list_identities>;

  /// Variant of all input events to session orchestrator
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

/// Request to display a message to the user
struct display_message
{
  std::string message;///< Message content to display
};

}// namespace radix_relay::core::events
