#include <algorithm>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/core/session_orchestrator.hpp>
#include <radix_relay/nostr/request_tracker.hpp>
#include <radix_relay/signal/signal_bridge.hpp>
#include <ranges>
#include <spdlog/spdlog.h>
#include <system_error>
#include <variant>

namespace radix_relay::core::test {

auto string_to_bytes(const std::string &str) -> std::vector<std::byte>
{
  std::vector<std::byte> bytes;
  bytes.resize(str.size());
  std::ranges::transform(str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
  return bytes;
}

struct queue_based_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::string db_path;
  std::shared_ptr<radix_relay::signal::bridge> bridge;
  std::shared_ptr<radix_relay::nostr::request_tracker> tracker;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> in_queue;
  std::shared_ptr<async::async_queue<events::transport::in_t>> transport_out_queue;
  std::shared_ptr<async::async_queue<events::transport_event_variant_t>> main_out_queue;
  std::shared_ptr<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>> orchestrator;

  explicit queue_based_fixture(std::string path) : db_path(std::move(path))
  {
    std::filesystem::remove(db_path);
    io_context = std::make_shared<boost::asio::io_context>();
    bridge = std::make_shared<radix_relay::signal::bridge>(db_path);
    tracker = std::make_shared<radix_relay::nostr::request_tracker>(io_context.get());
    in_queue = std::make_shared<async::async_queue<events::session_orchestrator::in_t>>(io_context);
    transport_out_queue = std::make_shared<async::async_queue<events::transport::in_t>>(io_context);
    main_out_queue = std::make_shared<async::async_queue<events::transport_event_variant_t>>(io_context);
    orchestrator =
      std::make_shared<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
        bridge, tracker, io_context, in_queue, transport_out_queue, main_out_queue);
  }

  // NOLINTNEXTLINE(bugprone-exception-escape)
  ~queue_based_fixture()
  {
    orchestrator.reset();
    io_context->run();
    bridge.reset();
    tracker.reset();
    std::error_code error_code;
    std::filesystem::remove(db_path, error_code);
  }

  queue_based_fixture(const queue_based_fixture &) = delete;
  auto operator=(const queue_based_fixture &) -> queue_based_fixture & = delete;
  queue_based_fixture(queue_based_fixture &&) = delete;
  auto operator=(queue_based_fixture &&) -> queue_based_fixture & = delete;
};

struct two_bridge_fixture
{
  std::string alice_db_path;
  std::string bob_db_path;
  std::shared_ptr<radix_relay::signal::bridge> alice_bridge;
  std::shared_ptr<radix_relay::signal::bridge> bob_bridge;
  std::string alice_rdx;
  std::string bob_rdx;

  std::shared_ptr<boost::asio::io_context> alice_io;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> alice_in;
  std::shared_ptr<async::async_queue<events::transport::in_t>> alice_transport_out;
  std::shared_ptr<async::async_queue<events::transport_event_variant_t>> alice_main_out;
  std::shared_ptr<radix_relay::nostr::request_tracker> alice_tracker;
  std::shared_ptr<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>> alice_orch;

  std::shared_ptr<boost::asio::io_context> bob_io;
  std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> bob_in;
  std::shared_ptr<async::async_queue<events::transport::in_t>> bob_transport_out;
  std::shared_ptr<async::async_queue<events::transport_event_variant_t>> bob_main_out;
  std::shared_ptr<radix_relay::nostr::request_tracker> bob_tracker;
  std::shared_ptr<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>> bob_orch;

  explicit two_bridge_fixture(std::string alice_path, std::string bob_path)
    : alice_db_path(std::move(alice_path)), bob_db_path(std::move(bob_path))
  {
    std::filesystem::remove(alice_db_path);
    std::filesystem::remove(bob_db_path);

    alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db_path);
    bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db_path);

    const auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    const auto alice_bundle_parsed = nlohmann::json::parse(alice_bundle_json);
    const std::string alice_bundle_base64 = alice_bundle_parsed["content"].template get<std::string>();

    const auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    const auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_bundle_parsed["content"].template get<std::string>();

    bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");
    alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    alice_io = std::make_shared<boost::asio::io_context>();
    alice_in = std::make_shared<async::async_queue<events::session_orchestrator::in_t>>(alice_io);
    alice_transport_out = std::make_shared<async::async_queue<events::transport::in_t>>(alice_io);
    alice_main_out = std::make_shared<async::async_queue<events::transport_event_variant_t>>(alice_io);
    alice_tracker = std::make_shared<radix_relay::nostr::request_tracker>(alice_io.get());
    alice_orch =
      std::make_shared<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
        alice_bridge, alice_tracker, alice_io, alice_in, alice_transport_out, alice_main_out);

    bob_io = std::make_shared<boost::asio::io_context>();
    bob_in = std::make_shared<async::async_queue<events::session_orchestrator::in_t>>(bob_io);
    bob_transport_out = std::make_shared<async::async_queue<events::transport::in_t>>(bob_io);
    bob_main_out = std::make_shared<async::async_queue<events::transport_event_variant_t>>(bob_io);
    bob_tracker = std::make_shared<radix_relay::nostr::request_tracker>(bob_io.get());
    bob_orch = std::make_shared<session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
      bob_bridge, bob_tracker, bob_io, bob_in, bob_transport_out, bob_main_out);
  }

  // NOLINTNEXTLINE(bugprone-exception-escape)
  ~two_bridge_fixture()
  {
    alice_orch.reset();
    bob_orch.reset();
    alice_io->run();
    bob_io->run();
    alice_bridge.reset();
    bob_bridge.reset();
    alice_tracker.reset();
    bob_tracker.reset();
    std::error_code error_code;
    std::filesystem::remove(alice_db_path, error_code);
    std::filesystem::remove(bob_db_path, error_code);
  }

  two_bridge_fixture(const two_bridge_fixture &) = delete;
  auto operator=(const two_bridge_fixture &) -> two_bridge_fixture & = delete;
  two_bridge_fixture(two_bridge_fixture &&) = delete;
  auto operator=(two_bridge_fixture &&) -> two_bridge_fixture & = delete;
};

auto bytes_to_string(const std::vector<std::byte> &bytes) -> std::string
{
  std::string result;
  result.resize(bytes.size());
  std::ranges::transform(bytes, result.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });
  return result;
}

SCENARIO("Queue-based session_orchestrator processes publish_identity command", "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_publish.db").string() };

    WHEN("run_once processes a publish_identity command from in_queue")
    {
      fixture.in_queue->push(events::publish_identity{});

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();
      fixture.io_context->restart();
      fixture.io_context->poll();

      THEN("A transport send command is emitted to transport_out_queue")
      {
        REQUIRE_FALSE(fixture.transport_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->size() == 1);
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes trust command", "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_trust.db").string() };

    WHEN("run_once processes a trust command from in_queue")
    {
      fixture.in_queue->push(events::trust{ .peer = "test_peer", .alias = "test_alias" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("No output events are generated (trust is stored internally)")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes bytes_received with unknown protocol",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{
      (std::filesystem::temp_directory_path() / "test_queue_bytes_unknown.db").string()
    };

    WHEN("run_once processes a bytes_received event with unknown protocol from in_queue")
    {
      const std::string json_msg = R"(["UNKNOWN","test"])";
      const auto bytes = string_to_bytes(json_msg);

      fixture.in_queue->push(events::transport::bytes_received{ bytes });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Session orchestrator processes the message without crashing")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes bytes_received with OK message",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_bytes_ok.db").string() };

    WHEN("run_once processes a bytes_received event with OK message from in_queue")
    {
      const std::string json_msg = R"(["OK","test_event_id",true,""])";
      const auto bytes = string_to_bytes(json_msg);

      fixture.in_queue->push(events::transport::bytes_received{ bytes });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Session orchestrator processes the OK message")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes bytes_received with EOSE message",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_bytes_eose.db").string() };

    WHEN("run_once processes a bytes_received event with EOSE message from in_queue")
    {
      const std::string json_msg = R"(["EOSE","test_subscription_id"])";
      const auto bytes = string_to_bytes(json_msg);

      fixture.in_queue->push(events::transport::bytes_received{ bytes });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Session orchestrator processes the EOSE message")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes transport connected event", "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_connected.db").string() };

    WHEN("run_once processes a transport connected event from in_queue")
    {
      fixture.in_queue->push(events::transport::connected{});

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Event is processed without output")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes transport disconnected event",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{
      (std::filesystem::temp_directory_path() / "test_queue_disconnected.db").string()
    };

    WHEN("run_once processes a transport disconnected event from in_queue")
    {
      fixture.in_queue->push(events::transport::disconnected{});

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Event is processed without output")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes transport sent event", "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_sent.db").string() };

    WHEN("run_once processes a transport sent event from in_queue")
    {
      fixture.in_queue->push(events::transport::sent{ .message_id = "test_msg_id" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Event is processed without output")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes transport send_failed event",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{
      (std::filesystem::temp_directory_path() / "test_queue_send_failed.db").string()
    };

    WHEN("run_once processes a transport send_failed event from in_queue")
    {
      fixture.in_queue->push(
        events::transport::send_failed{ .message_id = "test_msg_id", .error_message = "test reason" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Event is processed without output")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator processes transport connect_failed event",
  "[core][session_orchestrator][queue]")
{
  GIVEN("A session_orchestrator constructed with queues")
  {
    const queue_based_fixture fixture{
      (std::filesystem::temp_directory_path() / "test_queue_connect_failed.db").string()
    };

    WHEN("run_once processes a transport connect_failed event from in_queue")
    {
      fixture.in_queue->push(events::transport::connect_failed{ .url = "test_url", .error_message = "test reason" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();

      THEN("Event is processed without output")
      {
        REQUIRE(fixture.main_out_queue->empty());
        REQUIRE(fixture.transport_out_queue->empty());
      }
    }
  }
}

SCENARIO("Queue-based session_orchestrator Alice encrypts and Bob decrypts end-to-end",
  "[core][session_orchestrator][queue]")
{
  GIVEN("Alice and Bob orchestrators with established Signal Protocol sessions")
  {
    const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_e2e_alice.db").string(),
      (std::filesystem::temp_directory_path() / "test_queue_e2e_bob.db").string() };

    WHEN("Alice encrypts message and Bob receives it")
    {
      const std::string plaintext = "Hello Bob via queues!";
      const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

      auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

      std::string hex_content;
      for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

      constexpr std::uint64_t test_timestamp = 1234567890;
      constexpr std::uint32_t encrypted_message_kind = 40001;
      const nlohmann::json event_json = { { "id", "test_event_id_queue_e2e" },
        { "pubkey", fixture.alice_rdx },
        { "created_at", test_timestamp },
        { "kind", encrypted_message_kind },
        { "content", hex_content },
        { "sig", "signature" },
        { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", fixture.alice_rdx }) }) } };

      const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id_queue", event_json }).dump();

      fixture.bob_in->push(events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
      boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
      fixture.bob_io->run();

      THEN("Bob decrypts and receives the correct plaintext message")
      {
        REQUIRE_FALSE(fixture.bob_main_out->empty());
        REQUIRE(fixture.bob_main_out->size() == 1);

        fixture.bob_io->restart();
        auto main_future = boost::asio::co_spawn(*fixture.bob_io, fixture.bob_main_out->pop(), boost::asio::use_future);
        fixture.bob_io->run();
        auto main_event = main_future.get();

        REQUIRE(std::holds_alternative<events::message_received>(main_event));

        const auto &msg = std::get<events::message_received>(main_event);
        REQUIRE(msg.sender_rdx == fixture.alice_rdx);
        REQUIRE(msg.content == plaintext);
      }
    }
  }
}

TEST_CASE("session_orchestrator handles subscribe_identities command", "[session_orchestrator][subscribe]")
{
  GIVEN("A session orchestrator")
  {
    // NOLINTNEXTLINE(misc-const-correctness)
    queue_based_fixture fixture("/tmp/test_subscribe_identities.db");

    WHEN("subscribe_identities command is sent")
    {
      fixture.in_queue->push(events::subscribe_identities{ .subscription_id = "test_bundles" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();
      fixture.io_context->restart();
      fixture.io_context->run();

      THEN("it sends a subscription request for bundle announcements to transport")
      {
        REQUIRE(not fixture.transport_out_queue->empty());
        auto transport_cmd = fixture.transport_out_queue->try_pop();
        REQUIRE(transport_cmd.has_value());

        if (transport_cmd.has_value()) {
          REQUIRE(std::holds_alternative<events::transport::send>(transport_cmd.value()));

          const auto &send_cmd = std::get<events::transport::send>(transport_cmd.value());
          std::string json_str;
          json_str.resize(send_cmd.bytes.size());
          std::ranges::transform(
            send_cmd.bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

          auto parsed = nlohmann::json::parse(json_str);
          REQUIRE(parsed.is_array());
          REQUIRE(parsed[0] == "REQ");
          REQUIRE(parsed[1] == "test_bundles");
          REQUIRE(parsed[2].contains("kinds"));
          REQUIRE(parsed[2]["kinds"][0] == 30078);
          REQUIRE(parsed[2].contains("#d"));
          REQUIRE(parsed[2]["#d"][0] == "radix_prekey_bundle_v1");
        }
      }
    }
  }
}

TEST_CASE("session_orchestrator handles subscribe_messages command", "[session_orchestrator][subscribe]")
{
  GIVEN("A session orchestrator")
  {
    // NOLINTNEXTLINE(misc-const-correctness)
    queue_based_fixture fixture("/tmp/test_subscribe_messages.db");

    WHEN("subscribe_messages command is sent")
    {
      fixture.in_queue->push(events::subscribe_messages{ .subscription_id = "test_messages" });

      boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

      fixture.io_context->run();
      fixture.io_context->restart();
      fixture.io_context->run();

      THEN("it sends a subscription request for encrypted messages to transport")
      {
        REQUIRE(not fixture.transport_out_queue->empty());
        auto transport_cmd = fixture.transport_out_queue->try_pop();
        REQUIRE(transport_cmd.has_value());

        if (transport_cmd.has_value()) {
          REQUIRE(std::holds_alternative<events::transport::send>(transport_cmd.value()));

          const auto &send_cmd = std::get<events::transport::send>(transport_cmd.value());
          std::string json_str;
          json_str.resize(send_cmd.bytes.size());
          std::ranges::transform(
            send_cmd.bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

          auto parsed = nlohmann::json::parse(json_str);
          REQUIRE(parsed.is_array());
          REQUIRE(parsed[0] == "REQ");
          REQUIRE(parsed[1] == "test_messages");
          REQUIRE(parsed[2].contains("kinds"));
          REQUIRE(parsed[2]["kinds"][0] == 40001);
          REQUIRE(parsed[2].contains("#p"));
        }
      }
    }
  }
}

}// namespace radix_relay::core::test
