#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <radix_relay/events/events.hpp>
#include <radix_relay/nostr_request_tracker.hpp>
#include <radix_relay/nostr_transport.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <radix_relay/session_orchestrator.hpp>
#include <radix_relay/signal_bridge.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

namespace radix_relay {

constexpr std::size_t event_id_preview_length = 8;
constexpr auto bundle_wait_time = std::chrono::seconds(10);
constexpr auto message_wait_time = std::chrono::seconds(5);
constexpr auto subscription_wait_time = std::chrono::seconds(2);

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  std::cout << "Starting echo_demo_strand (Three-Strand Architecture)...\n";
  std::cout << "Radix Relay - Echo Demonstration with session_orchestrator\n";
  std::cout << "=========================================================\n\n";

  try {
    const auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    const auto alice_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                                / ("alice_strand_" + std::to_string(timestamp) + ".db"))
                                 .string();
    const auto bob_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                              / ("bob_strand_" + std::to_string(timestamp) + ".db"))
                               .string();

    std::cout << "Setting up three-strand architecture...\n";
    boost::asio::io_context io_context;
    boost::asio::io_context::strand main_strand{ io_context };
    const boost::asio::io_context::strand alice_session_strand{ io_context };
    boost::asio::io_context::strand alice_transport_strand{ io_context };
    const boost::asio::io_context::strand bob_session_strand{ io_context };
    boost::asio::io_context::strand bob_transport_strand{ io_context };

    std::cout << "Creating Signal Protocol bridges...\n";
    std::cout << "   Alice database: " << alice_db_path << "\n";
    std::cout << "   Bob database: " << bob_db_path << "\n";

    {
      auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db_path);
      auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db_path);

      std::cout << "\nCreating RequestTrackers...\n";
      auto alice_tracker = std::make_shared<radix_relay::nostr::request_tracker>(&io_context);
      auto bob_tracker = std::make_shared<radix_relay::nostr::request_tracker>(&io_context);

      std::vector<radix_relay::events::transport_event_variant_t> alice_events;
      std::vector<radix_relay::events::transport_event_variant_t> bob_events;

      std::string alice_peer_rdx;
      std::string bob_peer_rdx;

      std::cout << "\nCreating SessionOrchestrators and Transports...\n";

      // Forward declarations for circular dependency
      std::shared_ptr<
        radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
        alice_orch_ptr = nullptr;
      std::shared_ptr<
        radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
        bob_orch_ptr = nullptr;

      auto alice_send_to_session = [&alice_orch_ptr](const std::vector<std::byte> &bytes) {
        if (alice_orch_ptr) { alice_orch_ptr->handle_bytes_from_transport(bytes); }
      };

      auto bob_send_to_session = [&bob_orch_ptr](const std::vector<std::byte> &bytes) {
        if (bob_orch_ptr) { bob_orch_ptr->handle_bytes_from_transport(bytes); }
      };

      radix_relay::nostr::transport alice_transport(&io_context, alice_send_to_session);
      radix_relay::nostr::transport bob_transport(&io_context, bob_send_to_session);

      std::cout << "Connecting to Nostr relay...\n";
      try {
        alice_transport.connect("wss://relay.damus.io");
        std::cout << "   Alice connected to relay\n";

        bob_transport.connect("wss://relay.damus.io");
        std::cout << "   Bob connected to relay\n";
      } catch (const std::exception &e) {
        std::cout << "   Warning: Could not connect: " << e.what() << "\n";
        std::cout << "   Continuing with local demonstration...\n";
      }

      auto alice_send_bytes_to_transport = [&alice_transport_strand, &alice_transport](std::vector<std::byte> bytes) {
        boost::asio::post(alice_transport_strand, [&alice_transport, bytes = std::move(bytes)]() {
          alice_transport.send(std::span<const std::byte>(bytes.data(), bytes.size()));
        });
      };

      auto alice_send_event_to_main = [&main_strand, &alice_events, &alice_peer_rdx](
                                        radix_relay::events::transport_event_variant_t evt) {
        boost::asio::post(main_strand, [&alice_events, &alice_peer_rdx, evt = std::move(evt)]() {
          alice_events.push_back(evt);

          if (std::holds_alternative<radix_relay::events::message_received>(evt)) {
            const auto &msg = std::get<radix_relay::events::message_received>(evt);
            spdlog::info("[Alice] ✓ Received message from {}: {}", msg.sender_rdx, msg.content);
          } else if (std::holds_alternative<radix_relay::events::session_established>(evt)) {
            const auto &session = std::get<radix_relay::events::session_established>(evt);
            alice_peer_rdx = session.peer_rdx;
            spdlog::info("[Alice] ✓ Session established with peer: {}", alice_peer_rdx);
          } else if (std::holds_alternative<radix_relay::events::message_sent>(evt)) {
            const auto &sent = std::get<radix_relay::events::message_sent>(evt);
            if (sent.accepted) {
              spdlog::info("[Alice] ✓ Message sent successfully (event_id: {}...)",
                sent.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Alice] ✗ Message send failed");
            }
          } else if (std::holds_alternative<radix_relay::events::bundle_published>(evt)) {
            const auto &pub = std::get<radix_relay::events::bundle_published>(evt);
            if (pub.accepted) {
              spdlog::info(
                "[Alice] ✓ Bundle published (event_id: {}...)", pub.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Alice] ✗ Bundle publish failed");
            }
          } else if (std::holds_alternative<radix_relay::events::subscription_established>(evt)) {
            const auto &sub = std::get<radix_relay::events::subscription_established>(evt);
            if (!sub.subscription_id.empty()) {
              spdlog::info("[Alice] ✓ Subscription established: {}", sub.subscription_id);
            } else {
              spdlog::warn("[Alice] ✗ Subscription failed (timeout)");
            }
          }
        });
      };

      auto bob_send_bytes_to_transport = [&bob_transport_strand, &bob_transport](std::vector<std::byte> bytes) {
        boost::asio::post(bob_transport_strand, [&bob_transport, bytes = std::move(bytes)]() {
          bob_transport.send(std::span<const std::byte>(bytes.data(), bytes.size()));
        });
      };

      auto bob_send_event_to_main = [&main_strand, &bob_events, &bob_peer_rdx](
                                      radix_relay::events::transport_event_variant_t evt) {
        boost::asio::post(main_strand, [&bob_events, &bob_peer_rdx, evt = std::move(evt)]() {
          bob_events.push_back(evt);

          if (std::holds_alternative<radix_relay::events::message_received>(evt)) {
            const auto &msg = std::get<radix_relay::events::message_received>(evt);
            spdlog::info("[Bob] ✓ Received message from {}: {}", msg.sender_rdx, msg.content);
          } else if (std::holds_alternative<radix_relay::events::session_established>(evt)) {
            const auto &session = std::get<radix_relay::events::session_established>(evt);
            bob_peer_rdx = session.peer_rdx;
            spdlog::info("[Bob] ✓ Session established with peer: {}", bob_peer_rdx);
          } else if (std::holds_alternative<radix_relay::events::message_sent>(evt)) {
            const auto &sent = std::get<radix_relay::events::message_sent>(evt);
            if (sent.accepted) {
              spdlog::info("[Bob] ✓ Message sent successfully (event_id: {}...)",
                sent.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Bob] ✗ Message send failed");
            }
          } else if (std::holds_alternative<radix_relay::events::bundle_published>(evt)) {
            const auto &pub = std::get<radix_relay::events::bundle_published>(evt);
            if (pub.accepted) {
              spdlog::info(
                "[Bob] ✓ Bundle published (event_id: {}...)", pub.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Bob] ✗ Bundle publish failed");
            }
          } else if (std::holds_alternative<radix_relay::events::subscription_established>(evt)) {
            const auto &sub = std::get<radix_relay::events::subscription_established>(evt);
            if (!sub.subscription_id.empty()) {
              spdlog::info("[Bob] ✓ Subscription established: {}", sub.subscription_id);
            } else {
              spdlog::warn("[Bob] ✗ Subscription failed (timeout)");
            }
          }
        });
      };

      auto alice_orchestrator = std::make_shared<
        radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
        alice_bridge,
        alice_tracker,
        radix_relay::strands{
          .main = &main_strand, .session = &alice_session_strand, .transport = &alice_transport_strand },
        alice_send_bytes_to_transport,
        alice_send_event_to_main);

      auto bob_orchestrator = std::make_shared<
        radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(bob_bridge,
        bob_tracker,
        radix_relay::strands{
          .main = &main_strand, .session = &bob_session_strand, .transport = &bob_transport_strand },
        bob_send_bytes_to_transport,
        bob_send_event_to_main);

      // Wire up pointers for circular dependency
      alice_orch_ptr = alice_orchestrator;
      bob_orch_ptr = bob_orchestrator;

      std::thread io_thread([&io_context]() {
        spdlog::debug("io_context thread started");
        io_context.run();
        spdlog::debug("io_context thread stopped");
      });

      std::cout << "\n=== Phase 1: Subscribe to bundle announcements ===\n";
      const std::string alice_bundle_sub =
        R"(["REQ","alice_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";
      const std::string bob_bundle_sub = R"(["REQ","bob_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";

      alice_orchestrator->handle_command(radix_relay::events::subscribe{ .subscription_json = alice_bundle_sub });
      bob_orchestrator->handle_command(radix_relay::events::subscribe{ .subscription_json = bob_bundle_sub });

      std::this_thread::sleep_for(subscription_wait_time);

      std::cout << "\n=== Phase 2: Publish identity bundles ===\n";
      alice_orchestrator->handle_command(radix_relay::events::publish_identity{});
      bob_orchestrator->handle_command(radix_relay::events::publish_identity{});

      spdlog::info("Waiting for bundles to be received and sessions established...");
      std::this_thread::sleep_for(bundle_wait_time);

      std::cout << "\n=== Phase 3: Subscribe to encrypted messages ===\n";
      const auto alice_msg_sub =
        radix_relay::create_subscription_for_self(alice_bridge->get_rust_bridge(), "alice_msgs");
      const auto bob_msg_sub = radix_relay::create_subscription_for_self(bob_bridge->get_rust_bridge(), "bob_msgs");

      alice_orchestrator->handle_command(
        radix_relay::events::subscribe{ .subscription_json = std::string(alice_msg_sub) });
      bob_orchestrator->handle_command(radix_relay::events::subscribe{ .subscription_json = std::string(bob_msg_sub) });

      spdlog::info("Waiting for message subscriptions to establish...");
      std::this_thread::sleep_for(subscription_wait_time);

      if (!alice_peer_rdx.empty() && !bob_peer_rdx.empty()) {
        spdlog::info("[Bob] Assigning contact alias 'alice' to RDX: {}", bob_peer_rdx);
        bob_orchestrator->handle_command(radix_relay::events::trust{ .peer = bob_peer_rdx, .alias = "alice" });

        std::cout << "\n=== Phase 4: Exchange encrypted messages ===\n";

        const std::string alice_message = "Hello Bob! This is Alice sending you an encrypted message!";
        spdlog::info("[Alice] Sending message to {}", alice_peer_rdx);
        alice_orchestrator->handle_command(
          radix_relay::events::send{ .peer = alice_peer_rdx, .message = alice_message });

        std::this_thread::sleep_for(message_wait_time);

        const std::string bob_message = "Hi Alice! I received your message loud and clear!";
        spdlog::info("[Bob] Sending reply to 'alice' (was: {})", bob_peer_rdx);
        bob_orchestrator->handle_command(radix_relay::events::send{ .peer = "alice", .message = bob_message });

        std::this_thread::sleep_for(message_wait_time);

        std::cout << "\n=== Demonstration Complete ===\n";
        std::cout << "Alice received " << alice_events.size() << " events\n";
        std::cout << "Bob received " << bob_events.size() << " events\n";

        const auto alice_messages = std::count_if(alice_events.begin(), alice_events.end(), [](const auto &evt) {
          return std::holds_alternative<radix_relay::events::message_received>(evt);
        });
        const auto bob_messages = std::count_if(bob_events.begin(), bob_events.end(), [](const auto &evt) {
          return std::holds_alternative<radix_relay::events::message_received>(evt);
        });

        std::cout << "   Alice received " << alice_messages << " encrypted messages\n";
        std::cout << "   Bob received " << bob_messages << " encrypted messages\n";

        if (alice_messages > 0 && bob_messages > 0) {
          std::cout << "\n✓ SUCCESS: Messages were exchanged and decrypted successfully!\n";
        } else {
          std::cout << "\n✗ No messages were successfully exchanged\n";
        }
      } else {
        std::cout << "\n✗ Sessions were not established. Skipping message exchange.\n";
        std::cout << "   This is expected if not connected to a relay.\n";
      }

      std::cout << "\nStopping io_context...\n";
      io_context.stop();
      if (io_thread.joinable()) { io_thread.join(); }

      std::cout << "Cleaning up databases...\n";
    }

    std::filesystem::remove(alice_db_path);
    std::filesystem::remove(bob_db_path);

    std::cout << "echo_demo_strand completed successfully!\n";
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 1;
  }
}

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int { return radix_relay::main(); }
