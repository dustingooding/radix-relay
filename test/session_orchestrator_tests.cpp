#include "test_doubles/test_double_signal_bridge.hpp"
#include <algorithm>
#include <async/async_queue.hpp>
#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <core/events.hpp>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <nostr/protocol.hpp>
#include <nostr/request_tracker.hpp>
#include <nostr/session_orchestrator.hpp>
#include <platform/env_utils.hpp>
#include <ranges>
#include <signal/signal_bridge.hpp>
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

template<typename Bridge> struct orchestrator_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<Bridge> bridge;
  std::shared_ptr<radix_relay::nostr::request_tracker> tracker;
  std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> in_queue;
  std::shared_ptr<async::async_queue<core::events::transport::in_t>> transport_out_queue;
  std::shared_ptr<async::async_queue<core::events::presentation_event_variant_t>> presentation_out_queue;
  std::shared_ptr<radix_relay::nostr::session_orchestrator<Bridge, radix_relay::nostr::request_tracker>> orchestrator;
  std::string db_path;

  explicit orchestrator_fixture(std::string path = "") : db_path(std::move(path))
  {
    constexpr auto short_timeout{ 100 };

    if (not db_path.empty()) { std::filesystem::remove(db_path); }

    io_context = std::make_shared<boost::asio::io_context>();
    if constexpr (std::is_same_v<Bridge, radix_relay::signal::bridge>) {
      bridge = std::make_shared<Bridge>(db_path);
    } else {
      bridge = std::make_shared<Bridge>();
    }
    tracker = std::make_shared<radix_relay::nostr::request_tracker>(io_context);
    in_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);
    transport_out_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);
    presentation_out_queue =
      std::make_shared<async::async_queue<core::events::presentation_event_variant_t>>(io_context);
    orchestrator =
      std::make_shared<radix_relay::nostr::session_orchestrator<Bridge, radix_relay::nostr::request_tracker>>(bridge,
        tracker,
        io_context,
        in_queue,
        transport_out_queue,
        presentation_out_queue,
        std::chrono::milliseconds(short_timeout));
  }

  // NOLINTNEXTLINE(bugprone-exception-escape)
  ~orchestrator_fixture()
  {
    orchestrator.reset();
    tracker->cancel_all_pending();
    io_context->stop();
    bridge.reset();
    tracker.reset();
    if (not db_path.empty()) {
      std::error_code error_code;
      std::filesystem::remove(db_path, error_code);
    }
  }

  orchestrator_fixture(const orchestrator_fixture &) = delete;
  auto operator=(const orchestrator_fixture &) -> orchestrator_fixture & = delete;
  orchestrator_fixture(orchestrator_fixture &&) = delete;
  auto operator=(orchestrator_fixture &&) -> orchestrator_fixture & = delete;
};

using queue_based_fixture_t = orchestrator_fixture<radix_relay::signal::bridge>;
using test_double_fixture_t = orchestrator_fixture<radix_relay_test::test_double_signal_bridge>;

struct two_bridge_fixture
{
  std::string alice_db_path;
  std::string bob_db_path;
  std::shared_ptr<radix_relay::signal::bridge> alice_bridge;
  std::shared_ptr<radix_relay::signal::bridge> bob_bridge;
  std::string alice_rdx;
  std::string bob_rdx;

  std::shared_ptr<boost::asio::io_context> alice_io;
  std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> alice_in;
  std::shared_ptr<async::async_queue<core::events::transport::in_t>> alice_transport_out;
  std::shared_ptr<async::async_queue<core::events::presentation_event_variant_t>> alice_presentation_out;
  std::shared_ptr<radix_relay::nostr::request_tracker> alice_tracker;
  std::shared_ptr<
    radix_relay::nostr::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
    alice_orch;

  std::shared_ptr<boost::asio::io_context> bob_io;
  std::shared_ptr<async::async_queue<core::events::session_orchestrator::in_t>> bob_in;
  std::shared_ptr<async::async_queue<core::events::transport::in_t>> bob_transport_out;
  std::shared_ptr<async::async_queue<core::events::presentation_event_variant_t>> bob_presentation_out;
  std::shared_ptr<radix_relay::nostr::request_tracker> bob_tracker;
  std::shared_ptr<
    radix_relay::nostr::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
    bob_orch;

  explicit two_bridge_fixture(std::string alice_path, std::string bob_path)
    : alice_db_path(std::move(alice_path)), bob_db_path(std::move(bob_path))
  {
    constexpr auto short_timeout{ 100 };

    std::filesystem::remove(alice_db_path);
    std::filesystem::remove(bob_db_path);

    alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db_path);
    bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db_path);

    const auto alice_bundle_json_info = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    const auto alice_bundle_parsed = nlohmann::json::parse(alice_bundle_json_info.announcement_json);
    const std::string alice_bundle_base64 = alice_bundle_parsed["content"].template get<std::string>();

    const auto bob_bundle_json_info = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    const auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_json_info.announcement_json);
    const std::string bob_bundle_base64 = bob_bundle_parsed["content"].template get<std::string>();

    bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");
    alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    alice_io = std::make_shared<boost::asio::io_context>();
    alice_in = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(alice_io);
    alice_transport_out = std::make_shared<async::async_queue<core::events::transport::in_t>>(alice_io);
    alice_presentation_out = std::make_shared<async::async_queue<core::events::presentation_event_variant_t>>(alice_io);
    alice_tracker = std::make_shared<radix_relay::nostr::request_tracker>(alice_io);
    alice_orch = std::make_shared<
      radix_relay::nostr::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
      alice_bridge,
      alice_tracker,
      alice_io,
      alice_in,
      alice_transport_out,
      alice_presentation_out,
      std::chrono::milliseconds(short_timeout));

    bob_io = std::make_shared<boost::asio::io_context>();
    bob_in = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(bob_io);
    bob_transport_out = std::make_shared<async::async_queue<core::events::transport::in_t>>(bob_io);
    bob_presentation_out = std::make_shared<async::async_queue<core::events::presentation_event_variant_t>>(bob_io);
    bob_tracker = std::make_shared<radix_relay::nostr::request_tracker>(bob_io);
    bob_orch = std::make_shared<
      radix_relay::nostr::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(
      bob_bridge,
      bob_tracker,
      bob_io,
      bob_in,
      bob_transport_out,
      bob_presentation_out,
      std::chrono::milliseconds(short_timeout));
  }

  // NOLINTNEXTLINE(bugprone-exception-escape)
  ~two_bridge_fixture()
  {
    alice_orch.reset();
    bob_orch.reset();
    alice_tracker->cancel_all_pending();
    alice_io->stop();
    bob_tracker->cancel_all_pending();
    bob_io->stop();
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

TEST_CASE("Queue-based session_orchestrator processes publish_identity command", "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(events::publish_identity{});

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->poll();

  REQUIRE_FALSE(fixture.transport_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->size() == 1);
}

TEST_CASE("Queue-based session_orchestrator processes trust command", "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(events::trust{ .peer = "test_peer", .alias = "test_alias" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes bytes_received with unknown protocol",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  const std::string json_msg = R"(["UNKNOWN","test"])";
  const auto bytes = string_to_bytes(json_msg);

  fixture.in_queue->push(core::events::transport::bytes_received{ bytes });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes bytes_received with OK message",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  const std::string json_msg = R"(["OK","test_event_id",true,""])";
  const auto bytes = string_to_bytes(json_msg);

  fixture.in_queue->push(core::events::transport::bytes_received{ bytes });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes bytes_received with EOSE message",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  const std::string json_msg = R"(["EOSE","test_subscription_id"])";
  const auto bytes = string_to_bytes(json_msg);

  fixture.in_queue->push(core::events::transport::bytes_received{ bytes });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes transport disconnected event",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(core::events::transport::disconnected{});

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes transport sent event", "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(core::events::transport::sent{ .message_id = "test_msg_id" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes transport send_failed event",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(
    core::events::transport::send_failed{ .message_id = "test_msg_id", .error_message = "test reason" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator processes transport connect_failed event",
  "[core][session_orchestrator][queue]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(core::events::transport::connect_failed{ .url = "test_url", .error_message = "test reason" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();

  REQUIRE(fixture.presentation_out_queue->empty());
  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("Queue-based session_orchestrator Alice encrypts and Bob decrypts end-to-end",
  "[core][session_orchestrator][queue]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_queue_e2e_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_queue_e2e_bob.db").string() };

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

  fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
  fixture.bob_io->run();

  REQUIRE_FALSE(fixture.bob_presentation_out->empty());

  fixture.bob_io->restart();
  auto main_future =
    boost::asio::co_spawn(*fixture.bob_io, fixture.bob_presentation_out->pop(), boost::asio::use_future);
  fixture.bob_io->run();
  auto main_event = main_future.get();

  REQUIRE(std::holds_alternative<events::message_received>(main_event));

  const auto &msg = std::get<events::message_received>(main_event);
  REQUIRE(msg.sender_rdx == fixture.alice_rdx);
  REQUIRE(msg.content == plaintext);
}

TEST_CASE("session_orchestrator handles subscribe_identities command", "[session_orchestrator][subscribe]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(events::subscribe_identities{});

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->run();

  REQUIRE(not fixture.transport_out_queue->empty());
  auto transport_cmd = fixture.transport_out_queue->try_pop();
  REQUIRE(transport_cmd.has_value());

  if (transport_cmd.has_value()) {
    REQUIRE(std::holds_alternative<core::events::transport::send>(transport_cmd.value()));

    const auto &send_cmd = std::get<core::events::transport::send>(transport_cmd.value());
    std::string json_str;
    json_str.resize(send_cmd.bytes.size());
    std::ranges::transform(send_cmd.bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

    auto parsed = nlohmann::json::parse(json_str);
    REQUIRE(parsed.is_array());
    REQUIRE(parsed[0] == "REQ");

    const std::string subscription_id = parsed[1];
    REQUIRE(not subscription_id.empty());
    REQUIRE(subscription_id.length() <= 64);

    REQUIRE(parsed[2].contains("kinds"));
    REQUIRE(parsed[2]["kinds"][0] == 30078);
    REQUIRE(parsed[2].contains("#d"));
    REQUIRE(parsed[2]["#d"][0] == "radix_prekey_bundle_v1");
  }
}

TEST_CASE("session_orchestrator handles subscribe_messages command", "[session_orchestrator][subscribe]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(events::subscribe_messages{});

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);

  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->run();

  REQUIRE(not fixture.transport_out_queue->empty());
  auto transport_cmd = fixture.transport_out_queue->try_pop();
  REQUIRE(transport_cmd.has_value());

  if (transport_cmd.has_value()) {
    REQUIRE(std::holds_alternative<core::events::transport::send>(transport_cmd.value()));

    const auto &send_cmd = std::get<core::events::transport::send>(transport_cmd.value());
    std::string json_str;
    json_str.resize(send_cmd.bytes.size());
    std::ranges::transform(send_cmd.bytes, json_str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });

    auto parsed = nlohmann::json::parse(json_str);
    REQUIRE(parsed.is_array());
    CHECK_NOTHROW(parsed[0] == "REQ");

    const std::string subscription_id = parsed[1];
    CHECK(not subscription_id.empty());
    CHECK(subscription_id.length() <= 64);

    REQUIRE(parsed[2].contains("kinds"));
    CHECK(parsed[2]["kinds"][0] == 40001);
    CHECK(parsed[2].contains("#p"));
  }
}

TEST_CASE("session_orchestrator handles connect command and manages connection lifecycle",
  "[session_orchestrator][connect]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(events::connect{ .relay = "wss://relay.example.com" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);
  fixture.io_context->run();
  fixture.io_context->restart();

  REQUIRE(not fixture.transport_out_queue->empty());
  auto transport_cmd = fixture.transport_out_queue->try_pop();
  REQUIRE(transport_cmd.has_value());
  if (transport_cmd.has_value()) {
    REQUIRE(std::holds_alternative<core::events::transport::connect>(transport_cmd.value()));
    const auto &connect_cmd = std::get<core::events::transport::connect>(transport_cmd.value());
    REQUIRE(connect_cmd.url == "wss://relay.example.com");
  }

  REQUIRE(fixture.transport_out_queue->empty());
}

TEST_CASE("session_orchestrator sends subscriptions when transport connects", "[session_orchestrator][connect]")
{
  const test_double_fixture_t fixture;

  fixture.in_queue->push(core::events::transport::connected{ .url = "wss://relay.example.com" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);
  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->run();

  int req_count = 0;
  while (not fixture.transport_out_queue->empty()) {
    auto transport_cmd = fixture.transport_out_queue->try_pop();
    if (transport_cmd.has_value() and std::holds_alternative<core::events::transport::send>(*transport_cmd)) {
      const auto &send_cmd = std::get<core::events::transport::send>(*transport_cmd);
      const std::string json_str = bytes_to_string(send_cmd.bytes);
      auto parsed = nlohmann::json::parse(json_str);
      if (parsed.is_array() and parsed[0] == "REQ") { req_count++; }
    }
  }

  REQUIRE(req_count == 2);
}

TEST_CASE("session_orchestrator respects cancellation signal", "[core][session_orchestrator][cancellation]")
{
  struct test_state
  {
    std::atomic<bool> coroutine_done{ false };
  };

  const test_double_fixture_t fixture;
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());

  auto state = std::make_shared<test_state>();

  boost::asio::co_spawn(
    *fixture.io_context,
    [](std::shared_ptr<radix_relay::nostr::session_orchestrator<radix_relay_test::test_double_signal_bridge,
         radix_relay::nostr::request_tracker>> orch,
      std::shared_ptr<test_state> test_state_ptr,
      std::shared_ptr<boost::asio::cancellation_slot> c_slot) -> boost::asio::awaitable<void> {
      try {
        co_await orch->run(c_slot);
      } catch (const boost::system::system_error &err) {
        if (err.code() != boost::asio::error::operation_aborted
            and err.code() != boost::asio::experimental::error::channel_cancelled
            and err.code() != boost::asio::experimental::error::channel_closed) {
          throw;
        }
      }
      test_state_ptr->coroutine_done = true;
    }(fixture.orchestrator, state, cancel_slot),
    boost::asio::detached);

  fixture.io_context->poll();

  cancel_signal->emit(boost::asio::cancellation_type::terminal);
  fixture.io_context->run();

  REQUIRE(state->coroutine_done);
}

TEST_CASE("session_orchestrator returns empty identities list initially", "[session_orchestrator][bundles]")
{
  const test_double_fixture_t alice;

  alice.in_queue->push(core::events::list_identities{});

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();

  auto response = alice.presentation_out_queue->try_pop();
  REQUIRE(response.has_value());

  if (response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::identities_listed>(*response));
    const auto &listed = std::get<core::events::identities_listed>(*response);
    REQUIRE(listed.identities.empty());
  }
}

TEST_CASE("session_orchestrator stores discovered bundle identities", "[session_orchestrator][bundles]")
{
  const auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const auto alice_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_storage_alice_" + std::to_string(timestamp) + ".db"))
      .string();
  const auto bob_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_storage_bob_" + std::to_string(timestamp) + ".db"))
      .string();
  const queue_based_fixture_t alice(alice_db_path);
  const queue_based_fixture_t bob(bob_db_path);

  auto bob_announcement_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  auto bob_announcement_json = nlohmann::json::parse(bob_announcement_info.announcement_json);
  auto bob_bundle_base64 = bob_announcement_json["content"].template get<std::string>();
  auto bob_pubkey = bob_announcement_json["pubkey"].template get<std::string>();
  auto bob_event_id = bob_announcement_json["id"].template get<std::string>();

  const core::events::session_orchestrator::in_t event = core::events::bundle_announcement_received{
    .pubkey = bob_pubkey, .bundle_content = bob_bundle_base64, .event_id = bob_event_id
  };

  alice.in_queue->push(event);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  alice.in_queue->push(core::events::list_identities{});

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();

  auto response = alice.presentation_out_queue->try_pop();
  REQUIRE(response.has_value());

  if (response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::identities_listed>(*response));
    const auto &listed = std::get<core::events::identities_listed>(*response);
    REQUIRE(listed.identities.size() == 1);
    REQUIRE(listed.identities[0].nostr_pubkey == bob_pubkey);
    REQUIRE(listed.identities[0].event_id == bob_event_id);
    REQUIRE(listed.identities[0].rdx_fingerprint.starts_with("RDX:"));
    REQUIRE(listed.identities[0].rdx_fingerprint.length() == 68);
  }
}

TEST_CASE("session_orchestrator removes bundle when bundle_announcement_removed event is received",
  "[session_orchestrator][bundles]")
{
  const auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const auto alice_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_removal_alice_" + std::to_string(timestamp) + ".db"))
      .string();
  const auto bob_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_removal_bob_" + std::to_string(timestamp) + ".db"))
      .string();
  const queue_based_fixture_t alice(alice_db_path);
  const queue_based_fixture_t bob(bob_db_path);

  auto bob_announcement_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  auto bob_announcement_json = nlohmann::json::parse(bob_announcement_info.announcement_json);
  auto bob_bundle_base64 = bob_announcement_json["content"].template get<std::string>();
  auto bob_pubkey = bob_announcement_json["pubkey"].template get<std::string>();
  auto bob_event_id = bob_announcement_json["id"].template get<std::string>();

  const core::events::session_orchestrator::in_t bundle_event = core::events::bundle_announcement_received{
    .pubkey = bob_pubkey, .bundle_content = bob_bundle_base64, .event_id = bob_event_id
  };

  alice.in_queue->push(bundle_event);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  alice.in_queue->push(core::events::list_identities{});

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  auto initial_response = alice.presentation_out_queue->try_pop();
  REQUIRE(initial_response.has_value());
  if (initial_response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::identities_listed>(*initial_response));
    REQUIRE(std::get<core::events::identities_listed>(*initial_response).identities.size() == 1);
  }

  const core::events::session_orchestrator::in_t removal_event =
    core::events::bundle_announcement_removed{ .pubkey = bob_pubkey, .event_id = "removal_event_id" };

  alice.in_queue->push(removal_event);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  alice.in_queue->push(core::events::list_identities{});

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();

  auto response = alice.presentation_out_queue->try_pop();
  REQUIRE(response.has_value());

  if (response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::identities_listed>(*response));
    const auto &listed = std::get<core::events::identities_listed>(*response);
    REQUIRE(listed.identities.empty());
  }
}

TEST_CASE("session_orchestrator updates bundle when duplicate bundle_announcement_received is received",
  "[session_orchestrator][bundles]")
{
  const auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const auto alice_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_update_alice_" + std::to_string(timestamp) + ".db"))
      .string();
  const auto bob_db_path =
    (std::filesystem::temp_directory_path() / ("test_bundle_update_bob_" + std::to_string(timestamp) + ".db")).string();
  const queue_based_fixture_t alice(alice_db_path);
  const queue_based_fixture_t bob(bob_db_path);

  auto bob_announcement_1_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  auto bob_announcement_1_json = nlohmann::json::parse(bob_announcement_1_info.announcement_json);
  auto bob_bundle_base64_1 = bob_announcement_1_json["content"].template get<std::string>();
  auto bob_pubkey = bob_announcement_1_json["pubkey"].template get<std::string>();
  auto bob_event_id_1 = bob_announcement_1_json["id"].template get<std::string>();

  const core::events::session_orchestrator::in_t bundle_event_1 = core::events::bundle_announcement_received{
    .pubkey = bob_pubkey, .bundle_content = bob_bundle_base64_1, .event_id = bob_event_id_1
  };

  alice.in_queue->push(bundle_event_1);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  auto bob_announcement_2_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  auto bob_announcement_2_json = nlohmann::json::parse(bob_announcement_2_info.announcement_json);
  auto bob_bundle_base64_2 = bob_announcement_2_json["content"].template get<std::string>();
  auto bob_event_id_2 = bob_announcement_2_json["id"].template get<std::string>();

  const core::events::session_orchestrator::in_t bundle_event_2 = core::events::bundle_announcement_received{
    .pubkey = bob_pubkey, .bundle_content = bob_bundle_base64_2, .event_id = bob_event_id_2
  };

  alice.in_queue->push(bundle_event_2);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  alice.in_queue->push(core::events::list_identities{});

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();

  auto response = alice.presentation_out_queue->try_pop();
  REQUIRE(response.has_value());

  if (response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::identities_listed>(*response));
    const auto &listed = std::get<core::events::identities_listed>(*response);
    REQUIRE(listed.identities.size() == 1);
    REQUIRE(listed.identities[0].nostr_pubkey == bob_pubkey);
    REQUIRE(listed.identities[0].event_id == bob_event_id_2);
  }
}

TEST_CASE("session_orchestrator establishes session when trusting discovered identity",
  "[session_orchestrator][trust][bundles]")
{
  const auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const auto alice_db_path =
    (std::filesystem::temp_directory_path() / ("test_trust_alice_" + std::to_string(timestamp) + ".db")).string();
  const auto bob_db_path =
    (std::filesystem::temp_directory_path() / ("test_trust_bob_" + std::to_string(timestamp) + ".db")).string();
  const queue_based_fixture_t alice(alice_db_path);
  const queue_based_fixture_t bob(bob_db_path);

  auto bob_announcement_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  auto bob_announcement_json = nlohmann::json::parse(bob_announcement_info.announcement_json);
  auto bob_bundle_base64 = bob_announcement_json["content"].template get<std::string>();
  auto bob_pubkey = bob_announcement_json["pubkey"].template get<std::string>();
  auto bob_event_id = bob_announcement_json["id"].template get<std::string>();
  auto bob_rdx = alice.bridge->extract_rdx_from_bundle_base64(bob_bundle_base64);

  const core::events::session_orchestrator::in_t bundle_event = core::events::bundle_announcement_received{
    .pubkey = bob_pubkey, .bundle_content = bob_bundle_base64, .event_id = bob_event_id
  };

  alice.in_queue->push(bundle_event);

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();
  alice.io_context->restart();

  alice.in_queue->push(core::events::trust{ .peer = bob_rdx, .alias = "Bob" });

  boost::asio::co_spawn(
    *alice.io_context,
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
    [&alice]() -> boost::asio::awaitable<void> { co_await alice.orchestrator->run_once(); },
    boost::asio::detached);

  alice.io_context->run();

  auto response = alice.presentation_out_queue->try_pop();

  REQUIRE(response.has_value());
  if (response.has_value()) {
    REQUIRE(std::holds_alternative<core::events::session_established>(*response));
    const auto &session = std::get<core::events::session_established>(*response);
    REQUIRE(session.peer_rdx == bob_rdx);
  }

  const auto contacts = alice.bridge->list_contacts();
  REQUIRE(contacts.size() == 1);
  REQUIRE(contacts[0].rdx_fingerprint == bob_rdx);
  REQUIRE(contacts[0].user_alias == "Bob");
  REQUIRE(contacts[0].has_active_session);
}

TEST_CASE("session_orchestrator ignores duplicate encrypted messages", "[session_orchestrator][messages][duplicate]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_duplicate_msg_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_duplicate_msg_bob.db").string() };

  const std::string plaintext = "Test message";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t encrypted_message_kind = 40001;
  const nlohmann::json event_json = { { "id", "duplicate_test_event_id" },
    { "pubkey", fixture.alice_rdx },
    { "created_at", test_timestamp },
    { "kind", encrypted_message_kind },
    { "content", hex_content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", fixture.alice_rdx }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id", event_json }).dump();

  fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
  fixture.bob_io->run();
  fixture.bob_io->restart();

  REQUIRE_FALSE(fixture.bob_presentation_out->empty());
  auto first_response = fixture.bob_presentation_out->try_pop();
  REQUIRE(first_response.has_value());
  if (first_response.has_value()) { REQUIRE(std::holds_alternative<events::message_received>(*first_response)); }

  if (not fixture.bob_in->empty()) {
    boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
    fixture.bob_io->run();
    fixture.bob_io->restart();

    if (not fixture.bob_transport_out->empty()) {
      auto bundle_publish = fixture.bob_transport_out->try_pop();
      if (bundle_publish.has_value() and std::holds_alternative<core::events::transport::send>(*bundle_publish)) {
        const auto &send_cmd = std::get<core::events::transport::send>(*bundle_publish);
        const std::string json_str = bytes_to_string(send_cmd.bytes);
        auto parsed = nlohmann::json::parse(json_str);
        if (parsed.is_array() and parsed[0] == "EVENT") {
          const std::string bundle_event_id = parsed[1]["id"].get<std::string>();
          const std::string ok_response = nlohmann::json::array({ "OK", bundle_event_id, true, "" }).dump();
          fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(ok_response) });
          boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
          fixture.bob_io->run();
          fixture.bob_io->restart();
        }
      }
    }
  }

  fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
  fixture.bob_io->run();

  while (not fixture.bob_presentation_out->empty()) {
    auto event = fixture.bob_presentation_out->try_pop();
    if (event.has_value()) { REQUIRE_FALSE(std::holds_alternative<events::message_received>(*event)); }
  }
}

TEST_CASE("session_orchestrator includes since filter when subscribing to messages",
  "[session_orchestrator][subscribe][since]")
{
  const test_double_fixture_t fixture;

  constexpr std::uint64_t test_timestamp = 1700000000;

  fixture.bridge->update_last_message_timestamp(test_timestamp);

  fixture.in_queue->push(events::subscribe_messages{});
  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);
  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->run();

  REQUIRE(not fixture.transport_out_queue->empty());
  auto transport_cmd = fixture.transport_out_queue->try_pop();
  REQUIRE(transport_cmd.has_value());

  if (transport_cmd.has_value()) {
    REQUIRE(std::holds_alternative<core::events::transport::send>(transport_cmd.value()));

    const auto &send_cmd = std::get<core::events::transport::send>(transport_cmd.value());
    const std::string json_str = bytes_to_string(send_cmd.bytes);

    auto parsed = nlohmann::json::parse(json_str);
    REQUIRE(parsed.is_array());
    REQUIRE(parsed[0] == "REQ");
    REQUIRE(parsed[2].contains("since"));
    REQUIRE(parsed[2]["since"] == test_timestamp);
  }
}

TEST_CASE("encrypted message event structure contains correct sender and recipient information",
  "[session_orchestrator][messages][structure]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_msg_structure_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_msg_structure_bob.db").string() };

  const std::string plaintext = "Test message structure";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint32_t test_timestamp = 1234567890;
  const auto signed_event_json =
    fixture.alice_bridge->create_and_sign_encrypted_message(fixture.bob_rdx, hex_content, test_timestamp, "test-0.1.0");

  auto event_parsed = nlohmann::json::parse(signed_event_json);

  REQUIRE(event_parsed.contains("pubkey"));
  const std::string sender_nostr_pubkey = event_parsed["pubkey"].template get<std::string>();
  REQUIRE(sender_nostr_pubkey.length() == 64);

  REQUIRE(event_parsed.contains("tags"));
  REQUIRE(event_parsed["tags"].is_array());

  auto p_tag =
    std::ranges::find_if(event_parsed["tags"], [](const auto &tag) { return tag.is_array() and tag[0] == "p"; });
  REQUIRE(p_tag != event_parsed["tags"].end());

  const std::string recipient_nostr_pubkey = (*p_tag)[1].template get<std::string>();
  REQUIRE(recipient_nostr_pubkey.length() == 64);
  REQUIRE(recipient_nostr_pubkey != sender_nostr_pubkey);
}

TEST_CASE("received encrypted message can be decrypted using sender's RDX identity",
  "[session_orchestrator][messages][decrypt]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_decrypt_rdx_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_decrypt_rdx_bob.db").string() };

  const std::string plaintext = "Decrypt test message";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint32_t test_timestamp = 1234567890;
  const auto signed_event_json =
    fixture.alice_bridge->create_and_sign_encrypted_message(fixture.bob_rdx, hex_content, test_timestamp, "test-0.1.0");

  auto event_parsed = nlohmann::json::parse(signed_event_json);
  const std::string alice_nostr_pubkey = event_parsed["pubkey"].template get<std::string>();

  auto alice_contact = fixture.bob_bridge->lookup_contact(alice_nostr_pubkey);
  REQUIRE(alice_contact.rdx_fingerprint == fixture.alice_rdx);
  REQUIRE(alice_contact.user_alias == "alice");

  std::vector<uint8_t> encrypted_bytes_from_event;
  const std::string content_hex = event_parsed["content"].template get<std::string>();
  constexpr int hex_base = 16;
  for (size_t i = 0; i < content_hex.length(); i += 2) {
    auto byte_string = content_hex.substr(i, 2);
    auto byte_value = static_cast<uint8_t>(std::stoul(byte_string, nullptr, hex_base));
    encrypted_bytes_from_event.push_back(byte_value);
  }

  auto decrypted_bytes = fixture.bob_bridge->decrypt_message(fixture.alice_rdx, encrypted_bytes_from_event);

  const std::string decrypted_content(decrypted_bytes.begin(), decrypted_bytes.end());
  REQUIRE(decrypted_content == plaintext);
}

TEST_CASE("received encrypted message produces message_received event with correct sender identification",
  "[session_orchestrator][messages][sender]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_sender_id_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_sender_id_bob.db").string() };

  const std::string plaintext = "Sender identification test";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint32_t test_timestamp = 1234567890;
  const auto signed_event_json =
    fixture.alice_bridge->create_and_sign_encrypted_message(fixture.bob_rdx, hex_content, test_timestamp, "test-0.1.0");

  auto event_parsed = nlohmann::json::parse(signed_event_json);

  const nlohmann::json nostr_event_message = nlohmann::json::array({ "EVENT", "test_sub_id", event_parsed });
  const std::string nostr_message_json = nostr_event_message.dump();

  fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
  fixture.bob_io->run();

  REQUIRE_FALSE(fixture.bob_presentation_out->empty());

  fixture.bob_io->restart();
  auto main_future =
    boost::asio::co_spawn(*fixture.bob_io, fixture.bob_presentation_out->pop(), boost::asio::use_future);
  fixture.bob_io->run();
  auto main_event = main_future.get();

  REQUIRE(std::holds_alternative<events::message_received>(main_event));

  const auto &msg = std::get<events::message_received>(main_event);
  REQUIRE(msg.content == plaintext);
  REQUIRE(msg.sender_rdx == fixture.alice_rdx);
}

TEST_CASE("session_orchestrator republishes bundle when encrypted message indicates pre-key consumption",
  "[session_orchestrator][republish][prekey]")
{
  const two_bridge_fixture fixture{ (std::filesystem::temp_directory_path() / "test_republish_alice.db").string(),
    (std::filesystem::temp_directory_path() / "test_republish_bob.db").string() };

  const std::string plaintext = "Message that consumes pre-key";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint32_t test_timestamp = 1234567890;
  const auto signed_event_json =
    fixture.alice_bridge->create_and_sign_encrypted_message(fixture.bob_rdx, hex_content, test_timestamp, "test-0.1.0");

  auto event_parsed = nlohmann::json::parse(signed_event_json);
  const nlohmann::json nostr_event_message = nlohmann::json::array({ "EVENT", "test_sub_id", event_parsed });
  const std::string nostr_message_json = nostr_event_message.dump();

  fixture.bob_in->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*fixture.bob_io, fixture.bob_orch->run_once(), boost::asio::detached);
  fixture.bob_io->run();

  REQUIRE_FALSE(fixture.bob_transport_out->empty());
  auto transport_cmd = fixture.bob_transport_out->try_pop();
  REQUIRE(transport_cmd.has_value());
  if (transport_cmd.has_value()) {
    REQUIRE(std::holds_alternative<core::events::transport::send>(*transport_cmd));
    const auto &send_cmd = std::get<core::events::transport::send>(*transport_cmd);
    const std::string json_str = bytes_to_string(send_cmd.bytes);
    auto parsed = nlohmann::json::parse(json_str);
    REQUIRE(parsed.is_array());
    REQUIRE(parsed[0] == "EVENT");
    REQUIRE(parsed[1].contains("kind"));
    REQUIRE(parsed[1]["kind"] == 30078);
  }
}

TEST_CASE("session_orchestrator republishes bundle on connection if keys rotated",
  "[session_orchestrator][maintenance][connect]")
{
  const test_double_fixture_t fixture;

  fixture.bridge->set_maintenance_result(
    { .signed_pre_key_rotated = true, .kyber_pre_key_rotated = false, .pre_keys_replenished = false });

  fixture.in_queue->push(core::events::transport::connected{ .url = "wss://relay.example.com" });

  boost::asio::co_spawn(*fixture.io_context, fixture.orchestrator->run_once(), boost::asio::detached);
  fixture.io_context->run();
  fixture.io_context->restart();
  fixture.io_context->run();

  bool found_bundle = false;
  while (not fixture.transport_out_queue->empty()) {
    auto transport_cmd = fixture.transport_out_queue->try_pop();
    if (transport_cmd.has_value() and std::holds_alternative<core::events::transport::send>(*transport_cmd)) {
      const auto &send_cmd = std::get<core::events::transport::send>(*transport_cmd);
      const std::string json_str = bytes_to_string(send_cmd.bytes);
      auto parsed = nlohmann::json::parse(json_str);
      if (parsed.is_array() and parsed[0] == "EVENT" and parsed[1].contains("kind")
          and parsed[1]["kind"] == nostr::protocol::kind::bundle_announcement) {
        found_bundle = true;
        break;
      }
    }
  }

  REQUIRE(found_bundle);
}

TEST_CASE("reply to unknown sender includes correct nostr pubkey in p tag",
  "[session_orchestrator][x3dh][unknown-sender][reply]")
{
  const queue_based_fixture_t alice{
    (std::filesystem::temp_directory_path() / "test_reply_unknown_alice.db").string()
  };
  const queue_based_fixture_t bob{ (std::filesystem::temp_directory_path() / "test_reply_unknown_bob.db").string() };

  // Get bundles
  const auto alice_bundle_info = alice.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  const auto alice_bundle_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
  const std::string alice_bundle_base64 = alice_bundle_json["content"].template get<std::string>();
  const std::string alice_nostr_pubkey = alice_bundle_json["pubkey"].template get<std::string>();

  const auto bob_bundle_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  const auto bob_bundle_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
  const std::string bob_nostr_pubkey = bob_bundle_json["pubkey"].template get<std::string>();

  // Bob establishes session with Alice (Alice has NOT established session with Bob)
  const std::string alice_rdx = bob.bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "Alice");

  // Bob sends message to Alice
  const std::string plaintext = "Hello Alice from unknown Bob!";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
  auto encrypted_bytes = bob.bridge->encrypt_message(alice_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  // Create Nostr event from Bob to Alice
  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t encrypted_message_kind = 40001;
  const nlohmann::json event_json = { { "id", "test_event_unknown_bob" },
    { "pubkey", bob_nostr_pubkey },
    { "created_at", test_timestamp },
    { "kind", encrypted_message_kind },
    { "content", hex_content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", alice_nostr_pubkey }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id_unknown", event_json }).dump();

  // Alice receives the message from unknown Bob
  alice.in_queue->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*alice.io_context, alice.orchestrator->run_once(), boost::asio::detached);
  alice.io_context->run();

  // Verify Alice received the message
  REQUIRE_FALSE(alice.presentation_out_queue->empty());
  alice.io_context->restart();
  auto msg_future =
    boost::asio::co_spawn(*alice.io_context, alice.presentation_out_queue->pop(), boost::asio::use_future);
  alice.io_context->run();
  auto msg_event = msg_future.get();

  REQUIRE(std::holds_alternative<events::message_received>(msg_event));
  const auto &received = std::get<events::message_received>(msg_event);
  REQUIRE(received.content == plaintext);
  REQUIRE(received.sender_alias.starts_with("Unknown-"));

  // Alice sends a reply to Bob using his Unknown alias
  const std::string reply = "Hi Bob!";
  alice.io_context->restart();
  alice.in_queue->push(events::send{ .peer = received.sender_alias, .message = reply });
  boost::asio::co_spawn(*alice.io_context, alice.orchestrator->run_once(), boost::asio::detached);
  alice.io_context->run();

  // Alice's transport queue should have messages (bundle republish first, then reply)
  REQUIRE_FALSE(alice.transport_out_queue->empty());

  // Pop the bundle republish message (kind 30078)
  alice.io_context->restart();
  auto bundle_future =
    boost::asio::co_spawn(*alice.io_context, alice.transport_out_queue->pop(), boost::asio::use_future);
  alice.io_context->run();
  auto bundle_cmd = bundle_future.get();
  REQUIRE(std::holds_alternative<core::events::transport::send>(bundle_cmd));

  // Now pop the actual reply message (kind 40001)
  REQUIRE_FALSE(alice.transport_out_queue->empty());
  alice.io_context->restart();
  auto reply_future =
    boost::asio::co_spawn(*alice.io_context, alice.transport_out_queue->pop(), boost::asio::use_future);
  alice.io_context->run();
  auto reply_cmd = reply_future.get();

  REQUIRE(std::holds_alternative<core::events::transport::send>(reply_cmd));
  const auto &send_cmd = std::get<core::events::transport::send>(reply_cmd);
  const std::string json_str = bytes_to_string(send_cmd.bytes);
  auto parsed = nlohmann::json::parse(json_str);

  // Verify it's an EVENT message
  REQUIRE(parsed.is_array());
  REQUIRE(parsed[0] == "EVENT");
  const auto &reply_event = parsed[1];

  // Verify it's an encrypted message, not a bundle
  REQUIRE(reply_event["kind"] == 40001);

  // Verify the p tag contains Bob's Nostr pubkey
  REQUIRE(reply_event.contains("tags"));
  auto p_tag = std::ranges::find_if(
    reply_event["tags"], [](const auto &tag) { return tag.is_array() and tag.size() >= 2 and tag[0] == "p"; });

  REQUIRE(p_tag != reply_event["tags"].end());
  const std::string recipient_nostr_pubkey = (*p_tag)[1].template get<std::string>();
  CHECK(recipient_nostr_pubkey == bob_nostr_pubkey);
}

TEST_CASE("trust updates alias for existing contact without bundle", "[session_orchestrator][trust][alias]")
{
  const queue_based_fixture_t alice{ (std::filesystem::temp_directory_path() / "test_trust_update_alias.db").string() };
  const queue_based_fixture_t bob{ (std::filesystem::temp_directory_path() / "test_trust_update_bob.db").string() };

  const auto alice_bundle_info = alice.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  const auto alice_bundle_json = nlohmann::json::parse(alice_bundle_info.announcement_json);
  const std::string alice_bundle_base64 = alice_bundle_json["content"].template get<std::string>();

  const auto bob_bundle_info = bob.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  const auto bob_bundle_json = nlohmann::json::parse(bob_bundle_info.announcement_json);
  const std::string bob_nostr_pubkey = bob_bundle_json["pubkey"].template get<std::string>();

  const std::string alice_rdx = bob.bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "Alice");

  const std::string plaintext = "Hello Alice from Bob!";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
  auto encrypted_bytes = bob.bridge->encrypt_message(alice_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t encrypted_message_kind = 40001;
  const nlohmann::json event_json = { { "id", "test_event_bob" },
    { "pubkey", bob_nostr_pubkey },
    { "created_at", test_timestamp },
    { "kind", encrypted_message_kind },
    { "content", hex_content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", alice_rdx }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id", event_json }).dump();

  alice.in_queue->push(core::events::transport::bytes_received{ .bytes = string_to_bytes(nostr_message_json) });
  boost::asio::co_spawn(*alice.io_context, alice.orchestrator->run_once(), boost::asio::detached);
  alice.io_context->run();

  REQUIRE_FALSE(alice.presentation_out_queue->empty());
  alice.io_context->restart();
  auto msg_future =
    boost::asio::co_spawn(*alice.io_context, alice.presentation_out_queue->pop(), boost::asio::use_future);
  alice.io_context->run();
  auto msg_event = msg_future.get();

  REQUIRE(std::holds_alternative<events::message_received>(msg_event));
  const auto &received = std::get<events::message_received>(msg_event);
  REQUIRE(received.sender_alias.starts_with("Unknown-"));

  const std::string bob_rdx = received.sender_rdx;

  alice.io_context->restart();
  alice.in_queue->push(events::trust{ .peer = bob_rdx, .alias = "Bob" });
  boost::asio::co_spawn(*alice.io_context, alice.orchestrator->run_once(), boost::asio::detached);
  alice.io_context->run();

  auto updated_contact = alice.bridge->lookup_contact(bob_rdx);
  CHECK(updated_contact.user_alias == "Bob");
  CHECK(updated_contact.rdx_fingerprint == bob_rdx);
}

}// namespace radix_relay::core::test
