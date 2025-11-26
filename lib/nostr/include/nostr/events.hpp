#pragma once

#include <nostr/protocol.hpp>

namespace radix_relay::nostr::events {

/// Incoming Nostr event types
namespace incoming {

  /// Received bundle announcement event
  struct bundle_announcement : protocol::event_data
  {
    explicit bundle_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received identity announcement event
  struct identity_announcement : protocol::event_data
  {
    explicit identity_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received encrypted message event (kind 40001)
  struct encrypted_message : protocol::event_data
  {
    explicit encrypted_message(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received session establishment request
  struct session_request : protocol::event_data
  {
    explicit session_request(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received node status announcement
  struct node_status : protocol::event_data
  {
    explicit node_status(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received unknown/unrecognized message type
  struct unknown_message : protocol::event_data
  {
    explicit unknown_message(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Received OK response from relay
  struct ok : protocol::ok
  {
    explicit ok(const protocol::ok &msg) : protocol::ok(msg) {}
  };

  /// Received End of Stored Events marker
  struct eose : protocol::eose
  {
    explicit eose(const protocol::eose &msg) : protocol::eose(msg) {}
  };

  /// Received unknown protocol message
  struct unknown_protocol
  {
    std::string message;///< Raw message content
    explicit unknown_protocol(std::string msg) : message(std::move(msg)) {}
  };

}// namespace incoming

/// Outgoing Nostr event types
namespace outgoing {

  /// Bundle announcement to publish
  struct bundle_announcement : protocol::event_data
  {
    explicit bundle_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Identity announcement to publish
  struct identity_announcement : protocol::event_data
  {
    explicit identity_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Encrypted message to send
  struct encrypted_message : protocol::event_data
  {
    explicit encrypted_message(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Session establishment request to send
  struct session_request : protocol::event_data
  {
    explicit session_request(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  /// Plaintext message before encryption
  struct plaintext_message
  {
    std::string recipient;///< Recipient RDX fingerprint or alias
    std::string message;///< Message content

    plaintext_message(std::string recipient_id, std::string msg)
      : recipient(std::move(recipient_id)), message(std::move(msg))
    {}
  };

  /// Nostr subscription request
  struct subscription_request
  {
    std::string subscription_json;///< JSON-formatted subscription filter

    explicit subscription_request(std::string sub_json) : subscription_json(std::move(sub_json)) {}

    /**
     * @brief Extracts the subscription ID from the JSON.
     *
     * @return Subscription ID string
     */
    [[nodiscard]] auto get_subscription_id() const -> std::string;
  };

}// namespace outgoing

}// namespace radix_relay::nostr::events
