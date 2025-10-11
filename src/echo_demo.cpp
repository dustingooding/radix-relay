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
#include <spdlog/spdlog.h>
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
  ::rust::Box<::radix_relay::SignalBridge> bridge_;
  std::reference_wrapper<nostr::Transport> transport_;

public:
  DemoHandler(std::string name,
    std::string peer_name,
    ::rust::Box<::radix_relay::SignalBridge> bridge,
    nostr::Transport &transport)
    : name_(std::move(name)), peer_name_(std::move(peer_name)), bridge_(std::move(bridge)), transport_(transport)
  {}

  [[nodiscard]] auto bridge() -> ::radix_relay::SignalBridge & { return *bridge_; }
  [[nodiscard]] auto peer_name() const -> const std::string & { return peer_name_; }

  auto handle(const nostr::events::incoming::ok &event) const -> void
  {
    if (event.accepted) {
      spdlog::debug(
        "[{}] Message accepted by relay (event_id: {}...)", name_, event.event_id.substr(0, event_id_preview_length));
    } else {
      spdlog::warn("[{}] Message rejected by relay: {}", name_, event.message);
    }
  }

  auto handle(const nostr::events::incoming::eose &event) const -> void
  {
    spdlog::info("[{}] Received EOSE (End of Stored Events) for subscription: {}", name_, event.subscription_id);
    spdlog::info("[{}] Now listening for new real-time messages...", name_);
  }

  auto handle(const nostr::events::incoming::unknown_protocol &event) const -> void
  {
    spdlog::debug("[{}] Received unknown protocol message: {}", name_, event.message.substr(0, message_preview_length));
  }

  auto handle(const nostr::events::incoming::bundle_announcement &event) -> void
  {
    spdlog::info("[{}] Received bundle announcement from: {}", name_, event.pubkey);
    spdlog::debug("[{}] Bundle content length: {} bytes", name_, event.content.length());

    try {
      auto rdx = radix_relay::add_contact_and_establish_session_from_base64(*bridge_, event.content.c_str(), "");
      peer_name_ = std::string(rdx);
      spdlog::info("[{}] ✓ Established session with peer (RDX: {})", name_, peer_name_);
    } catch (const std::exception &e) {
      spdlog::warn("[{}] ✗ Failed to process bundle: {}", name_, e.what());
    }
  }

  auto handle(const nostr::events::incoming::identity_announcement &event) const -> void
  {
    spdlog::info("[{}] Received identity announcement from: {}", name_, event.pubkey);
  }

  auto handle(const nostr::events::incoming::encrypted_message &event) -> void
  {
    spdlog::info("[{}] Received encrypted message from: {}", name_, event.pubkey);
    constexpr std::size_t preview_length = 50;
    spdlog::debug("[{}] Encrypted content: {}...", name_, event.content.substr(0, preview_length));

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
      spdlog::info("[{}] Decrypted message: \"{}\"", name_, decrypted_message);

    } catch (const std::exception &e) {
      spdlog::error("[{}] Failed to decrypt message: {}", name_, e.what());
    }
  }

  auto handle(const nostr::events::incoming::session_request &event) const -> void
  {
    spdlog::info("[{}] Received session request from: {}", name_, event.pubkey);
  }

  auto handle(const nostr::events::incoming::node_status &event) const -> void
  {
    spdlog::info("[{}] Received node status from: {}", name_, event.pubkey);
  }

  auto handle(const nostr::events::incoming::unknown_message &event) const -> void
  {
    spdlog::debug("[{}] Received unknown event (kind {}) from: {}", name_, event.kind, event.pubkey);
  }

  auto handle(const nostr::events::outgoing::bundle_announcement &event,
    const std::function<void(const std::string &)> &track_fn = nullptr) const -> void
  {
    spdlog::debug(
      "[{}] Sending bundle announcement (event_id: {}...)", name_, event.id.substr(0, event_id_preview_length));
    if (track_fn) { track_fn(event.id); }
    send_event(event);
  }

  auto handle(const nostr::events::outgoing::identity_announcement &event,
    const std::function<void(const std::string &)> &track_fn = nullptr) const -> void
  {
    spdlog::debug(
      "[{}] Sending identity announcement (event_id: {}...)", name_, event.id.substr(0, event_id_preview_length));
    if (track_fn) { track_fn(event.id); }
    send_event(event);
  }

  auto handle(const nostr::events::outgoing::encrypted_message &event) const -> void { handle(event, nullptr); }

  auto handle(const nostr::events::outgoing::encrypted_message &event,
    const std::function<void(const std::string &)> &track_fn) const -> void
  {
    spdlog::debug(
      "[{}] Sending encrypted message (event_id: {}...)", name_, event.id.substr(0, event_id_preview_length));

    if (track_fn) { track_fn(event.id); }

    send_event(event);
  }

  auto handle(const nostr::events::outgoing::session_request &event,
    const std::function<void(const std::string &)> &track_fn = nullptr) const -> void
  {
    spdlog::debug("[{}] Sending session request (event_id: {}...)", name_, event.id.substr(0, event_id_preview_length));
    if (track_fn) { track_fn(event.id); }
    send_event(event);
  }

  auto handle(const nostr::events::outgoing::plaintext_message &event) -> void { handle(event, nullptr); }

  auto handle(const nostr::events::outgoing::plaintext_message &event,
    const std::function<void(const std::string &)> &track_fn) -> void
  {
    spdlog::info("[{}] Encrypting and sending message to {}: \"{}\"", name_, event.recipient, event.message);

    std::vector<uint8_t> message_bytes(event.message.begin(), event.message.end());
    auto signal_encrypted_bytes = radix_relay::encrypt_message(
      *bridge_, event.recipient, rust::Slice<const uint8_t>{ message_bytes.data(), message_bytes.size() });

    std::ostringstream oss;
    for (const auto &byte : signal_encrypted_bytes) {
      oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    const std::string encrypted_content_hex = oss.str();

    const auto event_timestamp = static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    auto signed_event_json = radix_relay::create_and_sign_encrypted_message(*bridge_,
      event.recipient,
      encrypted_content_hex,
      event_timestamp,
      std::string(radix_relay::cmake::project_version));

    auto signed_event = nostr::protocol::event_data::deserialize(std::string(signed_event_json));
    if (!signed_event.has_value()) {
      spdlog::error("[{}] Failed to create signed event", name_);
      return;
    }

    const nostr::events::outgoing::encrypted_message encrypted_event(*signed_event);

    if (track_fn) { track_fn(encrypted_event.id); }

    handle(encrypted_event);
  }

  auto handle(const nostr::events::outgoing::subscription_request &event) const -> void
  {
    spdlog::info("[{}] Subscribing with filter: {}", name_, event.subscription_json);

    std::vector<std::byte> bytes;
    bytes.reserve(event.subscription_json.size());
    std::ranges::transform(event.subscription_json, std::back_inserter(bytes), [](char character) {
      return static_cast<std::byte>(character);
    });
    transport_.get().send(bytes);
  }

private:
  auto send_event(const nostr::protocol::event_data &event) const -> void
  {
    auto protocol_event = nostr::protocol::event::from_event_data(event);
    auto json_str = protocol_event.serialize();

    std::vector<std::byte> bytes;
    bytes.resize(json_str.size());
    std::ranges::transform(json_str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
    transport_.get().send(bytes);
  }
};
// NOLINTEND(readability-convert-member-functions-to-static)
// cppcheck-suppress-end functionStatic

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  std::cout << "Starting echo_demo...\n" << std::flush;
  std::cout << "Radix Relay - Echo Demonstration\n" << std::flush;
  std::cout << "================================\n\n" << std::flush;

  try {
    std::cout << "Getting timestamp...\n" << std::flush;
    const auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::cout << "Setting up separate transports for Alice and Bob...\n" << std::flush;
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

      std::cout << "\nGenerating bundle announcement events...\n";
      const std::string version_str{ radix_relay::cmake::project_version };
      auto alice_bundle_event_str =
        radix_relay::generate_prekey_bundle_announcement(*alice_bridge, version_str.c_str());
      auto bob_bundle_event_str = radix_relay::generate_prekey_bundle_announcement(*bob_bridge, version_str.c_str());
      std::cout << "   Alice bundle announcement generated\n";
      std::cout << "   Bob bundle announcement generated\n";

      auto bob_subscription_req = radix_relay::create_subscription_for_self(*bob_bridge, "bob_sub");

      auto no_peer = std::string("");
      radix_relay::DemoHandler alice_handler("Alice", no_peer, std::move(alice_bridge), alice_transport);
      radix_relay::DemoHandler bob_handler("Bob", no_peer, std::move(bob_bridge), bob_transport);

      radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport> alice_session(
        alice_transport, alice_handler);

      radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport> bob_session(
        bob_transport, bob_handler);

      constexpr auto delivery_timeout = std::chrono::seconds(5);
      constexpr auto bundle_subscription_timeout = std::chrono::seconds(5);

      std::cout << "\n=== Phase 1: Alice and Bob exchange bundle announcements ===\n";

      auto alice_bundle_event =
        radix_relay::nostr::protocol::event_data::deserialize(std::string(alice_bundle_event_str));
      auto bob_bundle_event = radix_relay::nostr::protocol::event_data::deserialize(std::string(bob_bundle_event_str));

      const std::string alice_bundle_sub =
        R"(["REQ","alice_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";
      const std::string bob_bundle_sub = R"(["REQ","bob_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";

      if (alice_bundle_event && bob_bundle_event) {
        const radix_relay::nostr::events::outgoing::bundle_announcement alice_bundle{ *alice_bundle_event };
        const radix_relay::nostr::events::outgoing::bundle_announcement bob_bundle{ *bob_bundle_event };

        spdlog::info("Subscribing to bundle announcements...");
        const radix_relay::nostr::events::outgoing::subscription_request alice_sub{ alice_bundle_sub };
        const radix_relay::nostr::events::outgoing::subscription_request bob_sub{ bob_bundle_sub };

        boost::asio::co_spawn(
          alice_transport.io_context(),
          [](
            std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
              session_ref,
            radix_relay::nostr::events::outgoing::subscription_request event,
            std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
            try {
              auto eose = co_await session_ref.get().handle(event, timeout);
              spdlog::info("[Alice] ✓ Subscribed to bundles (EOSE: {})", eose.subscription_id);
            } catch (const std::exception &e) {
              spdlog::error("[Alice] ✗ Bundle subscription failed: {}", e.what());
            }
          }(std::ref(alice_session), alice_sub, bundle_subscription_timeout),
          boost::asio::detached);

        boost::asio::co_spawn(
          bob_transport.io_context(),
          [](
            std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
              session_ref,
            radix_relay::nostr::events::outgoing::subscription_request event,
            std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
            try {
              auto eose = co_await session_ref.get().handle(event, timeout);
              spdlog::info("[Bob] ✓ Subscribed to bundles (EOSE: {})", eose.subscription_id);
            } catch (const std::exception &e) {
              spdlog::error("[Bob] ✗ Bundle subscription failed: {}", e.what());
            }
          }(std::ref(bob_session), bob_sub, bundle_subscription_timeout),
          boost::asio::detached);

        constexpr auto subscription_setup_time = std::chrono::seconds(15);
        spdlog::info("Waiting {} seconds for stored bundles to arrive (waiting for EOSE or timeout)...",
          subscription_setup_time.count());
        std::this_thread::sleep_for(subscription_setup_time);
        spdlog::info("Proceeding with most recent bundle received");

        spdlog::info("Sending bundle announcements...");

        boost::asio::co_spawn(
          alice_transport.io_context(),
          [](
            std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
              session_ref,
            radix_relay::nostr::events::outgoing::bundle_announcement event,
            std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
            try {
              auto response = co_await session_ref.get().handle(event, timeout);
              if (response.accepted) {
                constexpr std::size_t preview_len = 8;
                spdlog::info(
                  "[Alice] ✓ Bundle announcement sent! Event ID: {}...", response.event_id.substr(0, preview_len));
              } else {
                spdlog::warn("[Alice] ✗ Bundle announcement rejected: {}", response.message);
              }
            } catch (const std::exception &e) {
              spdlog::error("[Alice] ✗ Bundle announcement failed: {}", e.what());
            }
          }(std::ref(alice_session), alice_bundle, delivery_timeout),
          boost::asio::detached);

        boost::asio::co_spawn(
          bob_transport.io_context(),
          [](
            std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
              session_ref,
            radix_relay::nostr::events::outgoing::bundle_announcement event,
            std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
            try {
              auto response = co_await session_ref.get().handle(event, timeout);
              if (response.accepted) {
                constexpr std::size_t preview_len = 8;
                spdlog::info(
                  "[Bob] ✓ Bundle announcement sent! Event ID: {}...", response.event_id.substr(0, preview_len));
              } else {
                spdlog::warn("[Bob] ✗ Bundle announcement rejected: {}", response.message);
              }
            } catch (const std::exception &e) {
              spdlog::error("[Bob] ✗ Bundle announcement failed: {}", e.what());
            }
          }(std::ref(bob_session), bob_bundle, delivery_timeout),
          boost::asio::detached);

        const auto bundle_subscription_timeout_plus_one = bundle_subscription_timeout + std::chrono::seconds{ 1 };
        spdlog::info(
          "Waiting {} seconds for our new bundles to be received...", bundle_subscription_timeout_plus_one.count());
        std::this_thread::sleep_for(bundle_subscription_timeout_plus_one);
        spdlog::info(
          "Using most recent bundle: Alice -> {}, Bob -> {}", alice_handler.peer_name(), bob_handler.peer_name());

        const std::string bob_rdx_to_alias = alice_handler.peer_name();
        spdlog::info("Alice assigning contact alias 'bob' to RDX: {}", bob_rdx_to_alias);
        radix_relay::assign_contact_alias(alice_handler.bridge(), bob_rdx_to_alias.c_str(), "bob");
      }

      std::cout << "\n=== Phase 2: Alice sends first message (while Bob is offline) ===\n";
      const std::string test_message = "Hello Bob! This is Alice sending you an encrypted message via Radix Relay!";
      const radix_relay::nostr::events::outgoing::plaintext_message msg_event{ alice_handler.peer_name(),
        test_message };

      boost::asio::co_spawn(
        alice_transport.io_context(),
        [](std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
             session_ref,
          radix_relay::nostr::events::outgoing::plaintext_message event,
          std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
          try {
            auto response = co_await session_ref.get().handle(event, timeout);
            if (response.accepted) {
              constexpr std::size_t preview_len = 8;
              spdlog::info(
                "[Alice] ✓ First message stored on relay! Event ID: {}...", response.event_id.substr(0, preview_len));
            } else {
              spdlog::warn("[Alice] ✗ First message rejected: {}", response.message);
            }
          } catch (const std::exception &e) {
            spdlog::error("[Alice] ✗ First message failed: {}", e.what());
          }
        }(std::ref(alice_session), msg_event, delivery_timeout),
        boost::asio::detached);

      constexpr auto storage_wait_time = std::chrono::seconds(2);
      std::this_thread::sleep_for(storage_wait_time);

      std::cout << "\n=== Phase 3: Bob comes online and subscribes ===\n";
      const radix_relay::nostr::events::outgoing::subscription_request sub_event{ std::string(bob_subscription_req) };

      boost::asio::co_spawn(
        bob_transport.io_context(),
        [](std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
             session_ref,
          radix_relay::nostr::events::outgoing::subscription_request event,
          std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
          try {
            auto eose = co_await session_ref.get().handle(event, timeout);
            spdlog::info("[Bob] ✓ Received EOSE for subscription: {}", eose.subscription_id);
            spdlog::info("[Bob] → All stored messages received! Now listening for real-time messages...");
          } catch (const std::exception &e) {
            spdlog::error("[Bob] ✗ Subscription failed: {}", e.what());
          }
        }(std::ref(bob_session), sub_event, delivery_timeout),
        boost::asio::detached);

      constexpr auto subscription_wait_time = std::chrono::seconds(2);
      std::this_thread::sleep_for(subscription_wait_time);

      std::cout << "\n=== Phase 4: Alice sends second message using alias (Bob receives in real-time) ===\n";
      const std::string test_message2 = "Bob, this is another message sent using your alias!";
      const radix_relay::nostr::events::outgoing::plaintext_message msg_event2{ "bob", test_message2 };

      boost::asio::co_spawn(
        alice_transport.io_context(),
        [](std::reference_wrapper<radix_relay::nostr::Session<radix_relay::DemoHandler, radix_relay::nostr::Transport>>
             session_ref,
          radix_relay::nostr::events::outgoing::plaintext_message event,
          std::chrono::milliseconds timeout) -> boost::asio::awaitable<void> {
          try {
            auto response = co_await session_ref.get().handle(event, timeout);
            if (response.accepted) {
              constexpr std::size_t preview_len = 8;
              spdlog::info(
                "[Alice] ✓ Second message delivered! Event ID: {}...", response.event_id.substr(0, preview_len));
            } else {
              spdlog::warn("[Alice] ✗ Second message rejected: {}", response.message);
            }
          } catch (const std::exception &e) {
            spdlog::error("[Alice] ✗ Second message failed: {}", e.what());
          }
        }(std::ref(alice_session), msg_event2, delivery_timeout),
        boost::asio::detached);

      std::cout << "Waiting for all operations to complete...\n";

      constexpr auto message_wait_time = std::chrono::seconds(3);
      std::this_thread::sleep_for(message_wait_time);

      std::cout << "Demo completed.\n";
    }

    std::filesystem::remove(alice_db_path);
    std::filesystem::remove(bob_db_path);

  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
