#pragma once

#include <nostr/protocol.hpp>

namespace radix_relay::nostr::events {

namespace incoming {

  struct bundle_announcement : protocol::event_data
  {
    explicit bundle_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct identity_announcement : protocol::event_data
  {
    explicit identity_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct encrypted_message : protocol::event_data
  {
    explicit encrypted_message(const protocol::event_data &event) : protocol::event_data(event) {}

    // Nostr Event (kind 40001):
    // pubkey: Sender's Nostr public key (32-byte hex)
    // tags.p: Recipient's Nostr public key
    // content: Hex-encoded encrypted message bytes
  };

  struct session_request : protocol::event_data
  {
    explicit session_request(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct node_status : protocol::event_data
  {
    explicit node_status(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct unknown_message : protocol::event_data
  {
    explicit unknown_message(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct ok : protocol::ok
  {
    explicit ok(const protocol::ok &msg) : protocol::ok(msg) {}
  };

  struct eose : protocol::eose
  {
    explicit eose(const protocol::eose &msg) : protocol::eose(msg) {}
  };

  struct unknown_protocol
  {
    std::string message;
    explicit unknown_protocol(std::string msg) : message(std::move(msg)) {}
  };

}// namespace incoming

namespace outgoing {

  struct bundle_announcement : protocol::event_data
  {
    explicit bundle_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct identity_announcement : protocol::event_data
  {
    explicit identity_announcement(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct encrypted_message : protocol::event_data
  {
    explicit encrypted_message(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct session_request : protocol::event_data
  {
    explicit session_request(const protocol::event_data &event) : protocol::event_data(event) {}
  };

  struct plaintext_message
  {
    std::string recipient;
    std::string message;

    plaintext_message(std::string recipient_id, std::string msg)
      : recipient(std::move(recipient_id)), message(std::move(msg))
    {}
  };

  struct subscription_request
  {
    std::string subscription_json;

    explicit subscription_request(std::string sub_json) : subscription_json(std::move(sub_json)) {}

    [[nodiscard]] auto get_subscription_id() const -> std::string;
  };

}// namespace outgoing

}// namespace radix_relay::nostr::events
