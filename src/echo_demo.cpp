#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <radix_relay/async/async_queue.hpp>
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
constexpr auto event_poll_interval = std::chrono::milliseconds(100);

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::cfg::load_env_levels();

  std::cout << "Starting echo_demo (Queue-Based Architecture)...\n";
  std::cout << "Radix Relay - Echo Demonstration with async_queue\n";
  std::cout << "=================================================\n\n";

  const auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

  const auto alice_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                              / ("alice_queue_" + std::to_string(timestamp) + ".db"))
                               .string();
  const auto bob_db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                            / ("bob_queue_" + std::to_string(timestamp) + ".db"))
                             .string();

  std::cout << "Setting up queue-based architecture...\n";
  auto io_context = std::make_shared<boost::asio::io_context>();

  std::cout << "Creating Signal Protocol bridges...\n";
  std::cout << "   Alice database: " << alice_db_path << "\n";
  std::cout << "   Bob database: " << bob_db_path << "\n";

  {
    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db_path);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db_path);

    std::cout << "\nCreating RequestTrackers...\n";
    auto alice_tracker = std::make_shared<radix_relay::nostr::request_tracker>(io_context.get());
    auto bob_tracker = std::make_shared<radix_relay::nostr::request_tracker>(io_context.get());

    std::vector<radix_relay::core::events::transport_event_variant_t> alice_events;
    std::vector<radix_relay::core::events::transport_event_variant_t> bob_events;

    std::string alice_peer_rdx;
    std::string bob_peer_rdx;
    bool alice_bundles_eose_received = false;
    bool bob_bundles_eose_received = false;

    auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("0.1.0");
    auto alice_bundle_parsed = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_own_pubkey = alice_bundle_parsed["pubkey"].get<std::string>();

    auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("0.1.0");
    auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_own_pubkey = bob_bundle_parsed["pubkey"].get<std::string>();

    std::cout << "\nCreating queues...\n";

    auto alice_session_in_queue =
      std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);
    auto alice_transport_in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto alice_main_event_queue =
      std::make_shared<async::async_queue<core::events::transport_event_variant_t>>(io_context);

    auto bob_session_in_queue =
      std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);
    auto bob_transport_in_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    auto bob_main_event_queue =
      std::make_shared<async::async_queue<core::events::transport_event_variant_t>>(io_context);

    std::cout << "\nCreating SessionOrchestrators and Transports...\n";

    auto alice_ws = std::make_shared<radix_relay::transport::websocket_stream>(*io_context);
    auto bob_ws = std::make_shared<radix_relay::transport::websocket_stream>(*io_context);

    auto alice_orchestrator = std::make_shared<
      radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
      alice_bridge,
      alice_tracker,
      io_context,
      alice_session_in_queue,
      alice_transport_in_queue,
      alice_main_event_queue);

    auto bob_orchestrator = std::make_shared<
      radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
      bob_bridge, bob_tracker, io_context, bob_session_in_queue, bob_transport_in_queue, bob_main_event_queue);

    auto alice_transport = std::make_shared<radix_relay::nostr::transport<radix_relay::transport::websocket_stream>>(
      alice_ws, io_context, alice_transport_in_queue, alice_session_in_queue);

    auto bob_transport = std::make_shared<radix_relay::nostr::transport<radix_relay::transport::websocket_stream>>(
      bob_ws, io_context, bob_transport_in_queue, bob_session_in_queue);

    auto work_guard = boost::asio::make_work_guard(*io_context);

    std::cout << "\nSpawning component processing loops...\n";

    auto spawn_orchestrator_loop =
      [](const std::shared_ptr<boost::asio::io_context> &ctx,
        std::shared_ptr<radix_relay::core::session_orchestrator<radix_relay::signal::bridge,
          radix_relay::nostr::request_tracker>> orch) {
        boost::asio::co_spawn(
          *ctx,
          // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
          [orchestrator = std::move(orch)]() -> boost::asio::awaitable<void> { co_await orchestrator->run(); },
          boost::asio::detached);
      };

    auto spawn_transport_loop =
      [](const std::shared_ptr<boost::asio::io_context> &ctx,
        std::shared_ptr<radix_relay::nostr::transport<radix_relay::transport::websocket_stream>> trans) {
        boost::asio::co_spawn(
          *ctx,
          // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
          [transport = std::move(trans)]() -> boost::asio::awaitable<void> { co_await transport->run(); },
          boost::asio::detached);
      };

    spawn_orchestrator_loop(io_context, alice_orchestrator);
    spawn_orchestrator_loop(io_context, bob_orchestrator);
    spawn_transport_loop(io_context, alice_transport);
    spawn_transport_loop(io_context, bob_transport);

    std::thread io_thread([&io_context]() {
      spdlog::debug("io_context thread started");
      io_context->run();
      spdlog::debug("io_context thread stopped");
    });

    std::cout << "Connecting to Nostr relay...\n";

    alice_transport_in_queue->push(radix_relay::core::events::transport::connect{ .url = "wss://relay.damus.io" });
    bob_transport_in_queue->push(radix_relay::core::events::transport::connect{ .url = "wss://relay.damus.io" });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n=== Phase 1: Subscribe to bundle announcements ===\n";
    alice_session_in_queue->push(radix_relay::core::events::subscribe_identities{ .subscription_id = "alice_bundles" });
    bob_session_in_queue->push(radix_relay::core::events::subscribe_identities{ .subscription_id = "bob_bundles" });

    std::this_thread::sleep_for(subscription_wait_time);

    std::cout << "\n=== Phase 2: Publish identity bundles ===\n";
    alice_session_in_queue->push(radix_relay::core::events::publish_identity{});
    bob_session_in_queue->push(radix_relay::core::events::publish_identity{});

    spdlog::info("Waiting for bundles to be received and sessions established...");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    while (not alice_main_event_queue->empty() or not bob_main_event_queue->empty()) {
      while (not alice_main_event_queue->empty()) {
        auto evt = alice_main_event_queue->try_pop();
        if (not evt) { break; }

        alice_events.push_back(*evt);

        if (std::holds_alternative<radix_relay::core::events::subscription_established>(*evt)) {
          const auto &sub = std::get<radix_relay::core::events::subscription_established>(*evt);
          if (sub.subscription_id == "alice_bundles") {
            alice_bundles_eose_received = true;
            spdlog::info("[Alice] Bundle subscription ready for real-time");
          }
        } else if (std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(*evt)) {
          const auto &bundle = std::get<radix_relay::core::events::bundle_announcement_received>(*evt);
          if (alice_bundles_eose_received and bundle.pubkey != alice_own_pubkey) {
            spdlog::info("[Alice] New bundle from {}, establishing session", bundle.pubkey);
            try {
              auto peer_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bundle.bundle_content, "");
              alice_peer_rdx = peer_rdx;
              spdlog::info("[Alice] Session established with peer: {}", alice_peer_rdx);
            } catch (const std::exception &e) {
              spdlog::debug("[Alice] Skipping bundle ({}): {}", bundle.pubkey, e.what());
            }
          }
        }
      }

      while (not bob_main_event_queue->empty()) {
        auto evt = bob_main_event_queue->try_pop();
        if (not evt) { break; }

        bob_events.push_back(*evt);

        if (std::holds_alternative<radix_relay::core::events::subscription_established>(*evt)) {
          const auto &sub = std::get<radix_relay::core::events::subscription_established>(*evt);
          if (sub.subscription_id == "bob_bundles") {
            bob_bundles_eose_received = true;
            spdlog::info("[Bob] Bundle subscription ready for real-time");
          }
        } else if (std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(*evt)) {
          const auto &bundle = std::get<radix_relay::core::events::bundle_announcement_received>(*evt);
          if (bob_bundles_eose_received and bundle.pubkey != bob_own_pubkey) {
            spdlog::info("[Bob] New bundle from {}, establishing session", bundle.pubkey);
            try {
              auto peer_rdx = bob_bridge->add_contact_and_establish_session_from_base64(bundle.bundle_content, "");
              bob_peer_rdx = peer_rdx;
              spdlog::info("[Bob] Session established with peer: {}", bob_peer_rdx);
            } catch (const std::exception &e) {
              spdlog::debug("[Bob] Skipping bundle ({}): {}", bundle.pubkey, e.what());
            }
          }
        }
      }

      std::this_thread::sleep_for(event_poll_interval);
    }

    std::cout << "\n=== Phase 3: Subscribe to encrypted messages ===\n";
    alice_session_in_queue->push(radix_relay::core::events::subscribe_messages{ .subscription_id = "alice_msgs" });
    bob_session_in_queue->push(radix_relay::core::events::subscribe_messages{ .subscription_id = "bob_msgs" });

    spdlog::info("Waiting for message subscriptions to establish...");
    std::this_thread::sleep_for(subscription_wait_time);

    if (not alice_peer_rdx.empty() and not bob_peer_rdx.empty()) {
      spdlog::info("[Bob] Assigning contact alias 'alice' to RDX: {}", bob_peer_rdx);
      bob_session_in_queue->push(radix_relay::core::events::trust{ .peer = bob_peer_rdx, .alias = "alice" });

      std::cout << "\n=== Phase 4: Exchange encrypted messages ===\n";

      const std::string alice_message = "Hello Bob! This is Alice sending you an encrypted message!";
      spdlog::info("[Alice] Sending message to {}", alice_peer_rdx);
      alice_session_in_queue->push(radix_relay::core::events::send{ .peer = alice_peer_rdx, .message = alice_message });

      std::this_thread::sleep_for(message_wait_time);

      while (not alice_main_event_queue->empty() or not bob_main_event_queue->empty()) {
        while (not alice_main_event_queue->empty()) {
          auto evt = alice_main_event_queue->try_pop();
          if (not evt) { break; }
          alice_events.push_back(*evt);

          if (std::holds_alternative<radix_relay::core::events::message_sent>(*evt)) {
            const auto &sent = std::get<radix_relay::core::events::message_sent>(*evt);
            if (sent.accepted) {
              spdlog::info("[Alice] Message sent successfully (event_id: {}...)",
                sent.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Alice] Message send failed");
            }
          }
        }

        while (not bob_main_event_queue->empty()) {
          auto evt = bob_main_event_queue->try_pop();
          if (not evt) { break; }
          bob_events.push_back(*evt);

          if (std::holds_alternative<radix_relay::core::events::message_received>(*evt)) {
            const auto &msg = std::get<radix_relay::core::events::message_received>(*evt);
            spdlog::info("[Bob] Received message from {}: {}", msg.sender_rdx, msg.content);
          }
        }

        std::this_thread::sleep_for(event_poll_interval);
      }

      const std::string bob_message = "Hi Alice! I received your message loud and clear!";
      spdlog::info("[Bob] Sending reply to 'alice' (was: {})", bob_peer_rdx);
      bob_session_in_queue->push(radix_relay::core::events::send{ .peer = "alice", .message = bob_message });

      std::this_thread::sleep_for(message_wait_time);

      while (not alice_main_event_queue->empty() or not bob_main_event_queue->empty()) {
        while (not alice_main_event_queue->empty()) {
          auto evt = alice_main_event_queue->try_pop();
          if (not evt) { break; }
          alice_events.push_back(*evt);

          if (std::holds_alternative<radix_relay::core::events::message_received>(*evt)) {
            const auto &msg = std::get<radix_relay::core::events::message_received>(*evt);
            spdlog::info("[Alice] Received message from {}: {}", msg.sender_rdx, msg.content);
          }
        }

        while (not bob_main_event_queue->empty()) {
          auto evt = bob_main_event_queue->try_pop();
          if (not evt) { break; }
          bob_events.push_back(*evt);

          if (std::holds_alternative<radix_relay::core::events::message_sent>(*evt)) {
            const auto &sent = std::get<radix_relay::core::events::message_sent>(*evt);
            if (sent.accepted) {
              spdlog::info(
                "[Bob] Message sent successfully (event_id: {}...)", sent.event_id.substr(0, event_id_preview_length));
            } else {
              spdlog::warn("[Bob] Message send failed");
            }
          }
        }

        std::this_thread::sleep_for(event_poll_interval);
      }

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
    io_context->stop();
    if (io_thread.joinable()) { io_thread.join(); }

    std::cout << "Cleaning up databases...\n";
  }

  std::filesystem::remove(alice_db_path);
  std::filesystem::remove(bob_db_path);

  std::cout << "echo_demo completed successfully!\n";
  return 0;
}

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int { return radix_relay::main(); }
