#include "internal_use_only/config.hpp"
#include "signal_bridge_cxx/lib.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <radix_relay/nostr.hpp>
#include <radix_relay/nostr_handler.hpp>
#include <radix_relay/nostr_transport.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace radix_relay {

// cppcheck-suppress-begin functionStatic
// NOLINTBEGIN(readability-convert-member-functions-to-static) - Required for NostrHandler concept
class DemoHandler
{
private:
  static constexpr std::size_t event_id_preview_length = 8;
  static constexpr std::size_t message_preview_length = 100;

  std::string name_;
  std::string peer_name_;
  mutable ::rust::Box<::radix_relay::SignalBridge> bridge_;

public:
  DemoHandler(std::string name, std::string peer_name, ::rust::Box<::radix_relay::SignalBridge> bridge)
    : name_(std::move(name)), peer_name_(std::move(peer_name)), bridge_(std::move(bridge))
  {}

  auto bridge() const -> ::radix_relay::SignalBridge & { return *bridge_; }

  auto handle(const nostr::events::incoming::ok &event) const -> void
  {
    if (event.accepted) {
      std::cout << name_
                << "'s message accepted by relay (event_id: " << event.event_id.substr(0, event_id_preview_length)
                << "...)\n";
    } else {
      std::cout << name_ << "'s message rejected by relay: " << event.message << "\n";
    }
  }

  auto handle(const nostr::events::incoming::eose &event) const -> void
  {
    std::cout << name_ << " received EOSE (End of Stored Events) for subscription: " << event.subscription_id << "\n";
    std::cout << "   Now listening for new real-time messages...\n";
  }

  auto handle(const nostr::events::incoming::unknown_protocol &event) const -> void
  {
    std::cout << name_ << " received unknown protocol message: " << event.message.substr(0, message_preview_length)
              << "\n";
  }

  auto handle(const nostr::events::incoming::identity_announcement &event) const -> void
  {
    std::cout << name_ << " received identity announcement from: " << event.pubkey << "\n";
  }

  auto handle(const nostr::events::incoming::encrypted_message &event) const -> void
  {
    std::cout << name_ << " received encrypted message from: " << event.pubkey << "\n";
    constexpr std::size_t preview_length = 50;
    std::cout << "Encrypted content: " << event.content.substr(0, preview_length) << "...\n";

    try {
      std::vector<uint8_t> encrypted_bytes;
      constexpr int hex_base = 16;
      for (size_t i = 0; i < event.content.length(); i += 2) {
        auto byte_string = event.content.substr(i, 2);
        auto byte_value = static_cast<uint8_t>(std::stoul(byte_string, nullptr, hex_base));
        encrypted_bytes.push_back(byte_value);
      }

      auto decrypted_bytes = radix_relay::decrypt_message(
        *bridge_, peer_name_, rust::Slice<const uint8_t>{ encrypted_bytes.data(), encrypted_bytes.size() });

      const std::string decrypted_message(decrypted_bytes.begin(), decrypted_bytes.end());
      std::cout << name_ << " decrypted message: \"" << decrypted_message << "\"\n";

    } catch (const std::exception &e) {
      std::cout << name_ << " failed to decrypt message: " << e.what() << "\n";
    }
  }

  auto handle(const nostr::events::incoming::session_request &event) const -> void
  {
    std::cout << name_ << " received session request from: " << event.pubkey << "\n";
  }

  auto handle(const nostr::events::incoming::node_status &event) const -> void
  {
    std::cout << name_ << " received node status from: " << event.pubkey << "\n";
  }

  auto handle(const nostr::events::incoming::unknown_message &event) const -> void
  {
    std::cout << name_ << " received unknown event (kind " << event.kind << ") from: " << event.pubkey << "\n";
  }

  auto handle(const nostr::events::outgoing::identity_announcement &event) const -> void
  {
    std::cout << name_ << " sending identity announcement (event_id: " << event.id.substr(0, event_id_preview_length)
              << "...)\n";
  }

  auto handle(const nostr::events::outgoing::encrypted_message &event) const -> void
  {
    std::cout << name_ << " sending encrypted message (event_id: " << event.id.substr(0, event_id_preview_length)
              << "...)\n";
  }

  auto handle(const nostr::events::outgoing::session_request &event) const -> void
  {
    std::cout << name_ << " sending session request (event_id: " << event.id.substr(0, event_id_preview_length)
              << "...)\n";
  }
};
// NOLINTEND(readability-convert-member-functions-to-static)
// cppcheck-suppress-end functionStatic

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
  std::cout << "Radix Relay - Echo Demonstration\n";
  std::cout << "================================\n\n";

  try {
    const auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << "Setting up separate transports for Alice and Bob...\n";
    radix_relay::nostr::Transport alice_transport;
    radix_relay::nostr::Transport bob_transport;

    std::cout << "Connecting Alice and Bob to Nostr relay...\n";
    try {
      alice_transport.connect("wss://relay.damus.io");
      std::cout << "   Alice successfully connected to WebSocket server\n";

      bob_transport.connect("wss://relay.damus.io");
      std::cout << "   Bob successfully connected to WebSocket server\n";
    } catch (const std::exception &e) {
      std::cout << "   Warning: Could not connect to WebSocket server: " << e.what() << "\n";
      std::cout << "   Continuing with local demonstration...\n";
    }

    std::cout << "Setting up Signal Protocol bridges for Alice and Bob...\n";
    const auto alice_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                                / ("alice_demo_" + std::to_string(timestamp) + ".db"))
                                 .string();
    const auto bob_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                              / ("bob_demo_" + std::to_string(timestamp) + ".db"))
                               .string();

    {
      auto alice_bridge = radix_relay::new_signal_bridge(alice_db_path.c_str());
      auto bob_bridge = radix_relay::new_signal_bridge(bob_db_path.c_str());
      std::cout << "   Alice database: " << alice_db_path << "\n";
      std::cout << "   Bob database: " << bob_db_path << "\n";

      auto alice_prekey_bundle = radix_relay::generate_pre_key_bundle(*alice_bridge);
      auto bob_prekey_bundle = radix_relay::generate_pre_key_bundle(*bob_bridge);

      radix_relay::establish_session(*alice_bridge, "bob", rust::Slice<const uint8_t>{ bob_prekey_bundle });
      std::cout << "   Alice established session with Bob\n";

      radix_relay::establish_session(*bob_bridge, "alice", rust::Slice<const uint8_t>{ alice_prekey_bundle });
      std::cout << "   Bob established session with Alice\n";

      radix_relay::DemoHandler alice_handler("Alice", "bob", std::move(alice_bridge));
      radix_relay::nostr::Session alice_session(alice_handler, alice_transport);

      std::cout << "Alice creating and encrypting test message for Bob...\n";
      const std::string test_message = "Hello Bob! This is Alice sending you an encrypted message via Radix Relay!";
      std::cout << "   Original message: \"" << test_message << "\"\n";

      std::vector<uint8_t> message_bytes(test_message.begin(), test_message.end());
      auto signal_encrypted_bytes = radix_relay::encrypt_message(
        alice_handler.bridge(), "bob", rust::Slice<const uint8_t>{ message_bytes.data(), message_bytes.size() });

      std::ostringstream oss;
      for (const auto &byte : signal_encrypted_bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
      }
      const std::string encrypted_content_hex = oss.str();

      const auto event_timestamp = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

      constexpr std::size_t preview_length = 50;
      std::cout << "   Encrypted content (hex): " << encrypted_content_hex.substr(0, preview_length) << "...\n";
      std::cout << "   Creating and signing Nostr event with all keys derived internally\n";

      auto signed_event_json = radix_relay::create_and_sign_encrypted_message(alice_handler.bridge(),
        "bob",
        encrypted_content_hex,
        event_timestamp,
        std::string(radix_relay::cmake::project_version));

      auto signed_event = radix_relay::nostr::protocol::event_data::deserialize(std::string(signed_event_json));
      if (!signed_event.has_value()) { throw std::runtime_error("Failed to deserialize signed event"); }

      std::cout << "Bob creating targeted subscription for messages addressed to him...\n";
      radix_relay::DemoHandler bob_handler("Bob", "alice", std::move(bob_bridge));
      auto subscription_req = radix_relay::create_subscription_for_self(bob_handler.bridge(), "bob_sub");
      std::cout << "   Subscription filter: " << std::string(subscription_req) << "\n";

      const radix_relay::nostr::Session bob_session(bob_handler, bob_transport);

      std::vector<std::byte> sub_bytes;
      sub_bytes.reserve(subscription_req.size());
      std::ranges::transform(subscription_req, std::back_inserter(sub_bytes), [](char character) {
        return static_cast<std::byte>(character);
      });
      bob_transport.send(std::span<const std::byte>(sub_bytes));

      constexpr auto subscription_wait_time = std::chrono::milliseconds(100);
      std::this_thread::sleep_for(subscription_wait_time);

      std::cout << "Alice sending encrypted message...\n";
      const radix_relay::nostr::events::outgoing::encrypted_message outgoing_event(*signed_event);
      alice_session.send(outgoing_event);

      std::cout << "Message sent to Nostr relay!\n";
      std::cout << "Waiting for responses from relay...\n";

      constexpr auto message_wait_time = std::chrono::seconds(5);
      std::this_thread::sleep_for(message_wait_time);

      std::cout << "Demo completed - check console output above for any received messages.\n";
    }

    std::filesystem::remove(alice_db_path);
    std::filesystem::remove(bob_db_path);

  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
