#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <radix_relay/core/events.hpp>
#include <radix_relay/core/session_orchestrator.hpp>
#include <radix_relay/nostr/request_tracker.hpp>
#include <radix_relay/nostr/transport.hpp>
#include <radix_relay/platform/env_utils.hpp>
#include <radix_relay/signal/signal_bridge.hpp>
#include <radix_relay/transport/websocket_stream.hpp>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

namespace radix_relay {

constexpr std::size_t event_id_preview_length = 8;
constexpr auto message_wait_time = std::chrono::seconds(5);
constexpr auto subscription_wait_time = std::chrono::seconds(2);

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::cfg::load_env_levels();

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

      std::vector<radix_relay::core::events::transport_event_variant_t> alice_events;
      std::vector<radix_relay::core::events::transport_event_variant_t> bob_events;

      std::string alice_peer_rdx;
      std::string bob_peer_rdx;
      bool alice_connected = false;
      bool bob_connected = false;
      bool alice_bundles_eose_received = false;
      bool bob_bundles_eose_received = false;

      // Get own nostr pubkeys to filter out self-bundles
      auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("0.1.0");
      auto alice_bundle_parsed = nlohmann::json::parse(alice_bundle_json);
      const std::string alice_own_pubkey = alice_bundle_parsed["pubkey"].get<std::string>();

      auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("0.1.0");
      auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_json);
      const std::string bob_own_pubkey = bob_bundle_parsed["pubkey"].get<std::string>();

      std::cout << "\nCreating SessionOrchestrators and Transports...\n";

      // Forward declarations for circular dependency
      std::shared_ptr<
        radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
        alice_orch_ptr = nullptr;
      std::shared_ptr<
        radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
        bob_orch_ptr = nullptr;

      // Keep io_context alive until explicitly stopped
      auto work_guard = boost::asio::make_work_guard(io_context);

      // Start io_context thread early to process async operations
      std::thread io_thread([&io_context]() {
        spdlog::debug("io_context thread started");
        io_context.run();
        spdlog::debug("io_context thread stopped");
      });

      auto alice_ws = std::make_shared<radix_relay::transport::websocket_stream>(io_context);
      auto bob_ws = std::make_shared<radix_relay::transport::websocket_stream>(io_context);

      // Event-driven transport: sends events TO session_strand
      auto alice_send_transport_event = [&alice_session_strand, &alice_orch_ptr, &alice_connected](
                                          radix_relay::core::events::transport::event_variant_t evt) {
        std::visit(
          [&alice_session_strand, &alice_orch_ptr, &alice_connected](auto &&event) {
            using event_t = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::bytes_received>) {
              boost::asio::post(alice_session_strand, [&alice_orch_ptr, event]() noexcept {
                if (alice_orch_ptr) { alice_orch_ptr->handle_event(event); }
              });
            } else if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::connected>) {
              alice_connected = true;
              std::cout << "   Alice connected to relay\n";
            } else if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::connect_failed>) {
              std::cout << "   Alice connection failed: " << event.error_message << "\n";
            }
          },
          evt);
      };

      auto bob_send_transport_event = [&bob_session_strand, &bob_orch_ptr, &bob_connected](
                                        radix_relay::core::events::transport::event_variant_t evt) {
        std::visit(
          [&bob_session_strand, &bob_orch_ptr, &bob_connected](auto &&event) {
            using event_t = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::bytes_received>) {
              boost::asio::post(bob_session_strand, [&bob_orch_ptr, event]() noexcept {
                if (bob_orch_ptr) { bob_orch_ptr->handle_event(event); }
              });
            } else if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::connected>) {
              bob_connected = true;
              std::cout << "   Bob connected to relay\n";
            } else if constexpr (std::is_same_v<event_t, radix_relay::core::events::transport::connect_failed>) {
              std::cout << "   Bob connection failed: " << event.error_message << "\n";
            }
          },
          evt);
      };

      radix_relay::nostr::transport alice_transport(
        alice_ws, io_context, &alice_session_strand, alice_send_transport_event);
      radix_relay::nostr::transport bob_transport(bob_ws, io_context, &bob_session_strand, bob_send_transport_event);

      std::cout << "Connecting to Nostr relay...\n";

      // Post connect commands to transport_strand
      boost::asio::post(alice_transport_strand, [&alice_transport]() noexcept {
        alice_transport.handle_command(radix_relay::core::events::transport::connect{ .url = "wss://relay.damus.io" });
      });

      boost::asio::post(bob_transport_strand, [&bob_transport]() noexcept {
        bob_transport.handle_command(radix_relay::core::events::transport::connect{ .url = "wss://relay.damus.io" });
      });

      // TODO: Replace polling with proper event handling - for now keep polling for acceptance test
      constexpr auto connection_poll_interval = std::chrono::milliseconds(10);
      constexpr auto connection_timeout = std::chrono::seconds(5);
      const auto start_time = std::chrono::steady_clock::now();
      while ((not alice_connected or not bob_connected)
             and (std::chrono::steady_clock::now() - start_time < connection_timeout)) {
        std::this_thread::sleep_for(connection_poll_interval);
        // Process io_context to allow connection events
      }

      if (not alice_connected or not bob_connected) {
        std::cout << "   Warning: Connection timeout - Alice: " << (alice_connected ? "connected" : "not connected")
                  << ", Bob: " << (bob_connected ? "connected" : "not connected") << "\n";
      }

      // Event-driven command sender: orchestrator sends commands TO transport_strand
      auto alice_send_transport_command = [&alice_transport_strand, &alice_transport](
                                            const radix_relay::core::events::transport::command_variant_t &cmd) {
        boost::asio::post(alice_transport_strand, [&alice_transport, cmd]() {// NOLINT(bugprone-exception-escape)
          try {
            std::visit([&alice_transport](auto &&command) { alice_transport.handle_command(command); }, cmd);
          } catch (const std::exception &e) {
            spdlog::error("[alice] Failed to process transport command: {}", e.what());
          }
        });
      };

      auto bob_send_transport_command = [&bob_transport_strand, &bob_transport](
                                          const radix_relay::core::events::transport::command_variant_t &cmd) {
        boost::asio::post(bob_transport_strand, [&bob_transport, cmd]() {// NOLINT(bugprone-exception-escape)
          try {
            std::visit([&bob_transport](auto &&command) { bob_transport.handle_command(command); }, cmd);
          } catch (const std::exception &e) {
            spdlog::error("[bob] Failed to process transport command: {}", e.what());
          }
        });
      };

      auto alice_send_event_to_main =
        [&main_strand, &alice_events, &alice_peer_rdx, &alice_bundles_eose_received, &alice_bridge, &alice_own_pubkey](
          radix_relay::core::events::transport_event_variant_t evt) {
          boost::asio::post(main_strand,
            [&alice_events,
              &alice_peer_rdx,
              &alice_bundles_eose_received,
              &alice_bridge,
              &alice_own_pubkey,
              evt = std::move(evt)]() {
              alice_events.push_back(evt);

              if (std::holds_alternative<radix_relay::core::events::message_received>(evt)) {
                const auto &msg = std::get<radix_relay::core::events::message_received>(evt);
                spdlog::info("[Alice] ✓ Received message from {}: {}", msg.sender_rdx, msg.content);
              } else if (std::holds_alternative<radix_relay::core::events::session_established>(evt)) {
                const auto &session = std::get<radix_relay::core::events::session_established>(evt);
                alice_peer_rdx = session.peer_rdx;
                spdlog::info("[Alice] ✓ Session established with peer: {}", alice_peer_rdx);
              } else if (std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(evt)) {
                const auto &bundle = std::get<radix_relay::core::events::bundle_announcement_received>(evt);
                if (alice_bundles_eose_received) {
                  // Check if this is our own bundle
                  if (bundle.pubkey == alice_own_pubkey) {
                    spdlog::debug("[Alice] Ignoring own bundle");
                    return;
                  }
                  spdlog::info("[Alice] ✓ New bundle from {}, establishing session", bundle.pubkey);
                  try {
                    // Establish session explicitly
                    auto peer_rdx =
                      alice_bridge->add_contact_and_establish_session_from_base64(bundle.bundle_content, "");
                    alice_peer_rdx = peer_rdx;
                    spdlog::info("[Alice] ✓ Session established with peer: {}", alice_peer_rdx);
                  } catch (const std::exception &e) {
                    spdlog::debug("[Alice] Skipping bundle ({}): {}", bundle.pubkey, e.what());
                  }
                } else {
                  spdlog::debug("[Alice] Ignoring pre-EOSE bundle from {}", bundle.pubkey);
                }
              } else if (std::holds_alternative<radix_relay::core::events::message_sent>(evt)) {
                const auto &sent = std::get<radix_relay::core::events::message_sent>(evt);
                if (sent.accepted) {
                  spdlog::info("[Alice] ✓ Message sent successfully (event_id: {}...)",
                    sent.event_id.substr(0, event_id_preview_length));
                } else {
                  spdlog::warn("[Alice] ✗ Message send failed");
                }
              } else if (std::holds_alternative<radix_relay::core::events::bundle_published>(evt)) {
                const auto &pub = std::get<radix_relay::core::events::bundle_published>(evt);
                if (pub.accepted) {
                  spdlog::info(
                    "[Alice] ✓ Bundle published (event_id: {}...)", pub.event_id.substr(0, event_id_preview_length));
                } else {
                  spdlog::warn("[Alice] ✗ Bundle publish failed");
                }
              } else if (std::holds_alternative<radix_relay::core::events::subscription_established>(evt)) {
                const auto &sub = std::get<radix_relay::core::events::subscription_established>(evt);
                if (not sub.subscription_id.empty()) {
                  // Track EOSE for bundle subscription
                  if (sub.subscription_id == "alice_bundles") {
                    alice_bundles_eose_received = true;
                    spdlog::info("[Alice] Bundle subscription ready for real-time");
                  }
                } else {
                  spdlog::warn("[Alice] ✗ Subscription failed (timeout)");
                }
              }
            });
        };

      auto bob_send_event_to_main =
        [&main_strand, &bob_events, &bob_peer_rdx, &bob_bundles_eose_received, &bob_bridge, &bob_own_pubkey](
          radix_relay::core::events::transport_event_variant_t evt) {
          boost::asio::post(main_strand,
            [&bob_events,
              &bob_peer_rdx,
              &bob_bundles_eose_received,
              &bob_bridge,
              &bob_own_pubkey,
              evt = std::move(evt)]() {
              bob_events.push_back(evt);

              if (std::holds_alternative<radix_relay::core::events::message_received>(evt)) {
                const auto &msg = std::get<radix_relay::core::events::message_received>(evt);
                spdlog::info("[Bob] ✓ Received message from {}: {}", msg.sender_rdx, msg.content);
              } else if (std::holds_alternative<radix_relay::core::events::session_established>(evt)) {
                const auto &session = std::get<radix_relay::core::events::session_established>(evt);
                bob_peer_rdx = session.peer_rdx;
                spdlog::info("[Bob] ✓ Session established with peer: {}", bob_peer_rdx);
              } else if (std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(evt)) {
                const auto &bundle = std::get<radix_relay::core::events::bundle_announcement_received>(evt);
                if (bob_bundles_eose_received) {
                  // Check if this is our own bundle
                  if (bundle.pubkey == bob_own_pubkey) {
                    spdlog::debug("[Bob] Ignoring own bundle");
                    return;
                  }
                  spdlog::info("[Bob] ✓ New bundle from {}, establishing session", bundle.pubkey);
                  try {
                    // Establish session explicitly
                    auto peer_rdx =
                      bob_bridge->add_contact_and_establish_session_from_base64(bundle.bundle_content, "");
                    bob_peer_rdx = peer_rdx;
                    spdlog::info("[Bob] ✓ Session established with peer: {}", bob_peer_rdx);
                  } catch (const std::exception &e) {
                    spdlog::debug("[Bob] Skipping bundle ({}): {}", bundle.pubkey, e.what());
                  }
                } else {
                  spdlog::debug("[Bob] Ignoring pre-EOSE bundle from {}", bundle.pubkey);
                }
              } else if (std::holds_alternative<radix_relay::core::events::message_sent>(evt)) {
                const auto &sent = std::get<radix_relay::core::events::message_sent>(evt);
                if (sent.accepted) {
                  spdlog::info("[Bob] ✓ Message sent successfully (event_id: {}...)",
                    sent.event_id.substr(0, event_id_preview_length));
                } else {
                  spdlog::warn("[Bob] ✗ Message send failed");
                }
              } else if (std::holds_alternative<radix_relay::core::events::bundle_published>(evt)) {
                const auto &pub = std::get<radix_relay::core::events::bundle_published>(evt);
                if (pub.accepted) {
                  spdlog::info(
                    "[Bob] ✓ Bundle published (event_id: {}...)", pub.event_id.substr(0, event_id_preview_length));
                } else {
                  spdlog::warn("[Bob] ✗ Bundle publish failed");
                }
              } else if (std::holds_alternative<radix_relay::core::events::subscription_established>(evt)) {
                const auto &sub = std::get<radix_relay::core::events::subscription_established>(evt);
                if (not sub.subscription_id.empty()) {
                  // Track EOSE for bundle subscription
                  if (sub.subscription_id == "bob_bundles") {
                    bob_bundles_eose_received = true;
                    spdlog::info("[Bob] Bundle subscription ready for real-time");
                  }
                } else {
                  spdlog::warn("[Bob] ✗ Subscription failed (timeout)");
                }
              }
            });
        };

      auto alice_orchestrator = std::make_shared<
        radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
        alice_bridge,
        alice_tracker,
        radix_relay::core::strands{
          .main = &main_strand, .session = &alice_session_strand, .transport = &alice_transport_strand },
        alice_send_transport_command,
        alice_send_event_to_main);

      auto bob_orchestrator = std::make_shared<
        radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
        bob_bridge,
        bob_tracker,
        radix_relay::core::strands{
          .main = &main_strand, .session = &bob_session_strand, .transport = &bob_transport_strand },
        bob_send_transport_command,
        bob_send_event_to_main);

      // Wire up pointers for circular dependency
      alice_orch_ptr = alice_orchestrator;
      bob_orch_ptr = bob_orchestrator;

      std::cout << "\n=== Phase 1: Subscribe to bundle announcements ===\n";
      const std::string alice_bundle_sub =
        R"(["REQ","alice_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";
      const std::string bob_bundle_sub = R"(["REQ","bob_bundles",{"kinds":[30078],"#d":["radix_prekey_bundle_v1"]}])";

      alice_orchestrator->handle_command(radix_relay::core::events::subscribe{ .subscription_json = alice_bundle_sub });
      bob_orchestrator->handle_command(radix_relay::core::events::subscribe{ .subscription_json = bob_bundle_sub });

      std::this_thread::sleep_for(subscription_wait_time);

      std::cout << "\n=== Phase 2: Publish identity bundles ===\n";
      alice_orchestrator->handle_command(radix_relay::core::events::publish_identity{});
      bob_orchestrator->handle_command(radix_relay::core::events::publish_identity{});

      spdlog::info("Waiting for bundles to be received and sessions established...");
      // With EOSE-aware processing, sessions establish immediately when new bundles arrive
      std::this_thread::sleep_for(std::chrono::seconds(2));

      std::cout << "\n=== Phase 3: Subscribe to encrypted messages ===\n";
      const auto alice_msg_sub =
        radix_relay::create_subscription_for_self(alice_bridge->get_rust_bridge(), "alice_msgs");
      const auto bob_msg_sub = radix_relay::create_subscription_for_self(bob_bridge->get_rust_bridge(), "bob_msgs");

      alice_orchestrator->handle_command(
        radix_relay::core::events::subscribe{ .subscription_json = std::string(alice_msg_sub) });
      bob_orchestrator->handle_command(
        radix_relay::core::events::subscribe{ .subscription_json = std::string(bob_msg_sub) });

      spdlog::info("Waiting for message subscriptions to establish...");
      std::this_thread::sleep_for(subscription_wait_time);

      if (not alice_peer_rdx.empty() and not bob_peer_rdx.empty()) {
        spdlog::info("[Bob] Assigning contact alias 'alice' to RDX: {}", bob_peer_rdx);
        bob_orchestrator->handle_command(radix_relay::core::events::trust{ .peer = bob_peer_rdx, .alias = "alice" });

        std::cout << "\n=== Phase 4: Exchange encrypted messages ===\n";

        const std::string alice_message = "Hello Bob! This is Alice sending you an encrypted message!";
        spdlog::info("[Alice] Sending message to {}", alice_peer_rdx);
        alice_orchestrator->handle_command(
          radix_relay::core::events::send{ .peer = alice_peer_rdx, .message = alice_message });

        std::this_thread::sleep_for(message_wait_time);

        const std::string bob_message = "Hi Alice! I received your message loud and clear!";
        spdlog::info("[Bob] Sending reply to 'alice' (was: {})", bob_peer_rdx);
        bob_orchestrator->handle_command(radix_relay::core::events::send{ .peer = "alice", .message = bob_message });

        std::this_thread::sleep_for(message_wait_time);

        std::cout << "\n=== Demonstration Complete ===\n";
        std::cout << "Alice received " << alice_events.size() << " events\n";
        std::cout << "Bob received " << bob_events.size() << " events\n";

        const auto alice_messages = std::count_if(alice_events.begin(), alice_events.end(), [](const auto &evt) {
          return std::holds_alternative<radix_relay::core::events::message_received>(evt);
        });
        const auto bob_messages = std::count_if(bob_events.begin(), bob_events.end(), [](const auto &evt) {
          return std::holds_alternative<radix_relay::core::events::message_received>(evt);
        });

        std::cout << "   Alice received " << alice_messages << " encrypted messages\n";
        std::cout << "   Bob received " << bob_messages << " encrypted messages\n";

        if (alice_messages > 0 and bob_messages > 0) {
          std::cout << "\n✓ SUCCESS: Messages were exchanged and decrypted successfully!\n";
        } else {
          std::cout << "\n✗ No messages were successfully exchanged\n";
        }
      } else {
        std::cout << "\n✗ Sessions were not established. Skipping message exchange.\n";
        std::cout << "   This is expected if not connected to a relay.\n";
      }

      std::cout << "\nStopping io_context...\n";
      work_guard.reset();
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
