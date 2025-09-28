#pragma once

#include <radix_relay/nostr_protocol.hpp>

namespace radix_relay::nostr::events {

namespace incoming {

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
    explicit unknown_protocol(const std::string &msg) : message(msg) {}
  };

}// namespace incoming

namespace outgoing {

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

}// namespace outgoing

}// namespace radix_relay::nostr::events
