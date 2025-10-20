#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <radix_relay/node_identity.hpp>
#include <radix_relay/nostr_request_tracker.hpp>
#include <radix_relay/session_orchestrator.hpp>
#include <radix_relay/signal_bridge.hpp>
#include <ranges>
#include <signal_bridge_cxx/lib.h>

namespace {

struct orchestrator_test_fixture
{
  boost::asio::io_context io_context;
  boost::asio::io_context::strand main_strand{ io_context };
  boost::asio::io_context::strand session_strand{ io_context };
  boost::asio::io_context::strand transport_strand{ io_context };
  std::shared_ptr<radix_relay::nostr::request_tracker> tracker{ std::make_shared<radix_relay::nostr::request_tracker>(
    &io_context) };
  mutable std::vector<std::byte> sent_bytes;
  mutable std::vector<radix_relay::events::transport_event_variant_t> main_events;

  auto make_send_bytes_to_transport() const -> std::function<void(std::vector<std::byte>)>
  {
    return [this](std::vector<std::byte> bytes) { sent_bytes = std::move(bytes); };
  }

  auto make_send_event_to_main() const -> std::function<void(radix_relay::events::transport_event_variant_t)>
  {
    return [this](radix_relay::events::transport_event_variant_t evt) { main_events.push_back(std::move(evt)); };
  }
};

struct single_bridge_fixture : orchestrator_test_fixture
{
  std::string db_path;
  std::shared_ptr<radix_relay::signal::bridge> bridge;

  explicit single_bridge_fixture(std::string path) : db_path(std::move(path))
  {
    std::filesystem::remove(db_path);
    bridge = std::make_shared<radix_relay::signal::bridge>(db_path);
  }

  ~single_bridge_fixture() noexcept
  {
    std::error_code error_code;
    std::filesystem::remove(db_path, error_code);
  }

  single_bridge_fixture(const single_bridge_fixture &) = delete;
  auto operator=(const single_bridge_fixture &) -> single_bridge_fixture & = delete;
  single_bridge_fixture(single_bridge_fixture &&) = delete;
  auto operator=(single_bridge_fixture &&) -> single_bridge_fixture & = delete;

  auto make_orchestrator() const -> std::shared_ptr<
    radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
  {
    if (!bridge) { throw std::runtime_error("Bridge not initialized"); }
    return std::make_shared<
      radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(bridge,
      tracker,
      radix_relay::strands{ .main = &main_strand, .session = &session_strand, .transport = &transport_strand },
      make_send_bytes_to_transport(),
      make_send_event_to_main());
  }
};

auto string_to_bytes(const std::string &str) -> std::vector<std::byte>
{
  std::vector<std::byte> bytes;
  bytes.resize(str.size());
  std::ranges::transform(str, bytes.begin(), [](char character) { return std::bit_cast<std::byte>(character); });
  return bytes;
}

auto bytes_to_string(const std::vector<std::byte> &bytes) -> std::string
{
  std::string str;
  str.reserve(bytes.size());
  std::ranges::transform(bytes, std::back_inserter(str), [](std::byte byte) { return static_cast<char>(byte); });
  return str;
}

auto make_event_message(std::uint32_t kind, const std::string &content = "test_content") -> std::string
{
  constexpr std::uint64_t test_timestamp = 1234567890;
  const nlohmann::json event_json = { { "id", "test_event_id" },
    { "pubkey", "test_pubkey" },
    { "created_at", test_timestamp },
    { "kind", kind },
    { "content", content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array() } };

  return nlohmann::json::array({ "EVENT", "sub_id", event_json }).dump();
}

struct two_bridge_fixture : orchestrator_test_fixture
{
  std::string alice_path;
  std::string bob_path;
  std::shared_ptr<radix_relay::signal::bridge> alice_bridge;
  std::shared_ptr<radix_relay::signal::bridge> bob_bridge;
  std::string bob_rdx;
  std::string alice_rdx;

  two_bridge_fixture(std::string alice_db, std::string bob_db, bool bidirectional = false)
    : alice_path(std::move(alice_db)), bob_path(std::move(bob_db))
  {
    std::filesystem::remove(alice_path);
    std::filesystem::remove(bob_path);

    alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_path);
    bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_path);

    auto bob_bundle_json = bob_bridge->generate_prekey_bundle_announcement("test-0.1.0");
    auto bob_event_json = nlohmann::json::parse(bob_bundle_json);
    const std::string bob_bundle_base64 = bob_event_json["content"].template get<std::string>();

    bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    if (bidirectional) {
      auto alice_bundle_json = alice_bridge->generate_prekey_bundle_announcement("test-0.1.0");
      auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
      const std::string alice_bundle_base64 = alice_event_json["content"].template get<std::string>();

      alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");
    }
  }

  ~two_bridge_fixture() noexcept
  {
    std::error_code error_code;
    std::filesystem::remove(alice_path, error_code);
    std::filesystem::remove(bob_path, error_code);
  }

  two_bridge_fixture(const two_bridge_fixture &) = delete;
  auto operator=(const two_bridge_fixture &) -> two_bridge_fixture & = delete;
  two_bridge_fixture(two_bridge_fixture &&) = delete;
  auto operator=(two_bridge_fixture &&) -> two_bridge_fixture & = delete;

  auto make_alice_orchestrator() const -> std::shared_ptr<
    radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
  {
    if (!alice_bridge) { throw std::runtime_error("Alice bridge not initialized"); }
    return std::make_shared<
      radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(alice_bridge,
      tracker,
      radix_relay::strands{
        .main = &main_strand,
        .session = &session_strand,
        .transport = &transport_strand,
      },
      make_send_bytes_to_transport(),
      make_send_event_to_main());
  }

  auto make_bob_orchestrator() const -> std::shared_ptr<
    radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
  {
    if (!bob_bridge) { throw std::runtime_error("Bob bridge not initialized"); }
    return std::make_shared<
      radix_relay::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(bob_bridge,
      tracker,
      radix_relay::strands{
        .main = &main_strand,
        .session = &session_strand,
        .transport = &transport_strand,
      },
      make_send_bytes_to_transport(),
      make_send_event_to_main());
  }

  auto get_alice_bridge() const -> std::shared_ptr<radix_relay::signal::bridge>
  {
    if (!alice_bridge) { throw std::runtime_error("Alice bridge not initialized"); }
    return alice_bridge;
  }

  auto get_bob_bridge() const -> std::shared_ptr<radix_relay::signal::bridge>
  {
    if (!bob_bridge) { throw std::runtime_error("Bob bridge not initialized"); }
    return bob_bridge;
  }

  auto get_alice_rdx() const -> const std::string &
  {
    if (alice_rdx.empty()) { throw std::runtime_error("Alice RDX not initialized - use bidirectional=true"); }
    return alice_rdx;
  }
};

}// namespace

TEST_CASE("session_orchestrator handles send command", "[session_orchestrator]")
{
  two_bridge_fixture fixture("/tmp/session_orch_send_alice.db", "/tmp/session_orch_send_bob.db");
  auto orchestrator = fixture.make_alice_orchestrator();

  const std::string plaintext = "Hello Bob!";
  orchestrator->handle_command(radix_relay::events::send{ .peer = std::string(fixture.bob_rdx), .message = plaintext });

  fixture.io_context.run();

  REQUIRE_FALSE(fixture.sent_bytes.empty());

  auto parsed = nlohmann::json::parse(bytes_to_string(fixture.sent_bytes));

  CHECK(parsed.is_array());
  CHECK(parsed[0] == "EVENT");
  CHECK(parsed[1]["kind"] == 40001);
  CHECK_FALSE(parsed[1]["content"].get<std::string>().empty());
}

TEST_CASE("session_orchestrator handles publish_identity command", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_publish_alice.db");
  auto orchestrator = fixture.make_orchestrator();

  orchestrator->handle_command(radix_relay::events::publish_identity{});

  fixture.io_context.run();

  REQUIRE_FALSE(fixture.sent_bytes.empty());

  auto parsed = nlohmann::json::parse(bytes_to_string(fixture.sent_bytes));

  CHECK(parsed.is_array());
  CHECK(parsed[0] == "EVENT");
  CHECK(parsed[1]["kind"] == 30078);
}

TEST_CASE("session_orchestrator handles trust command", "[session_orchestrator]")
{
  two_bridge_fixture fixture("/tmp/session_orch_trust_alice.db", "/tmp/session_orch_trust_bob.db");
  auto orchestrator = fixture.make_alice_orchestrator();

  const std::string alias = "bob_alias";
  orchestrator->handle_command(radix_relay::events::trust{ .peer = std::string(fixture.bob_rdx), .alias = alias });

  fixture.io_context.run();

  CHECK(fixture.sent_bytes.empty());

  auto contact = fixture.get_alice_bridge()->lookup_contact(alias);
  CHECK(std::string(contact.rdx_fingerprint) == std::string(fixture.bob_rdx));
  CHECK(std::string(contact.user_alias) == alias);
}

TEST_CASE("session_orchestrator handles incoming encrypted_message bytes", "[session_orchestrator]")
{
  two_bridge_fixture fixture("/tmp/session_orch_incoming_alice.db", "/tmp/session_orch_incoming_bob.db", true);
  auto orchestrator = fixture.make_bob_orchestrator();

  const std::string plaintext = "Hello Bob!";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());
  auto encrypted_bytes = fixture.get_alice_bridge()->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += fmt::format("{:02x}", byte); }

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t encrypted_message_kind = 40001;
  const nlohmann::json event_json = { { "id", "test_event_id" },
    { "pubkey", "alice_pubkey" },
    { "created_at", test_timestamp },
    { "kind", encrypted_message_kind },
    { "content", hex_content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", std::string(fixture.get_alice_rdx()) }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id", event_json }).dump();

  orchestrator->handle_bytes_from_transport(string_to_bytes(nostr_message_json));

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);

  CHECK(std::holds_alternative<radix_relay::events::message_received>(fixture.main_events[0]));
  const auto &msg = std::get<radix_relay::events::message_received>(fixture.main_events[0]);
  CHECK(msg.sender_rdx == std::string(fixture.get_alice_rdx()));
  CHECK(msg.content == plaintext);
  CHECK(msg.timestamp == test_timestamp);
}

TEST_CASE("session_orchestrator handles incoming bundle_announcement bytes", "[session_orchestrator]")
{
  two_bridge_fixture fixture("/tmp/session_orch_bundle_alice.db", "/tmp/session_orch_bundle_bob.db");
  auto orchestrator = fixture.make_bob_orchestrator();

  auto alice_bundle_json = fixture.get_alice_bridge()->generate_prekey_bundle_announcement("test-0.1.0");
  auto alice_event_json = nlohmann::json::parse(alice_bundle_json);
  const std::string alice_bundle_base64 = alice_event_json["content"].template get<std::string>();

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t bundle_announcement_kind = 30078;
  const nlohmann::json bundle_event_json = { { "id", "bundle_event_id" },
    { "pubkey", "alice_nostr_pubkey" },
    { "created_at", test_timestamp },
    { "kind", bundle_announcement_kind },
    { "content", alice_bundle_base64 },
    { "sig", "bundle_signature" },
    { "tags", nlohmann::json::array() } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id", bundle_event_json }).dump();

  orchestrator->handle_bytes_from_transport(string_to_bytes(nostr_message_json));

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);

  CHECK(std::holds_alternative<radix_relay::events::session_established>(fixture.main_events[0]));
  const auto &session_evt = std::get<radix_relay::events::session_established>(fixture.main_events[0]);
  CHECK(session_evt.peer_rdx.starts_with("RDX:"));
  CHECK(session_evt.peer_rdx.length() == 68);
}

TEST_CASE("session_orchestrator handles incoming OK message", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_ok.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string ok_message = R"(["OK","test_event_id",true,""])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(ok_message));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming EOSE message", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_eose.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string eose_message = R"(["EOSE","test_subscription_id"])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(eose_message));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming identity_announcement", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_identity.db");
  auto orchestrator = fixture.make_orchestrator();

  constexpr std::uint32_t identity_announcement_kind = 40002;
  orchestrator->handle_bytes_from_transport(string_to_bytes(make_event_message(identity_announcement_kind)));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming session_request", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_session_req.db");
  auto orchestrator = fixture.make_orchestrator();

  constexpr std::uint32_t session_request_kind = 40003;
  orchestrator->handle_bytes_from_transport(string_to_bytes(make_event_message(session_request_kind)));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming node_status", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_node_status.db");
  auto orchestrator = fixture.make_orchestrator();

  constexpr std::uint32_t node_status_kind = 40004;
  orchestrator->handle_bytes_from_transport(string_to_bytes(make_event_message(node_status_kind)));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming unknown_message", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_unknown_msg.db");
  auto orchestrator = fixture.make_orchestrator();

  constexpr std::uint32_t unknown_kind = 99999;
  orchestrator->handle_bytes_from_transport(string_to_bytes(make_event_message(unknown_kind)));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles incoming unknown_protocol", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_unknown_proto.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string unknown_protocol_message = R"(["UNKNOWN","some","stuff"])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(unknown_protocol_message));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator handles malformed JSON", "[session_orchestrator]")
{
  single_bridge_fixture fixture("/tmp/session_orch_malformed.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string malformed_json = "not valid json at all";
  orchestrator->handle_bytes_from_transport(string_to_bytes(malformed_json));

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator async tracking for send command - success", "[session_orchestrator][async_tracking]")
{
  two_bridge_fixture fixture("/tmp/session_orch_send_ok_alice.db", "/tmp/session_orch_send_ok_bob.db");
  auto orchestrator = fixture.make_alice_orchestrator();

  const std::string plaintext = "Hello Bob!";
  orchestrator->handle_command(radix_relay::events::send{ .peer = std::string(fixture.bob_rdx), .message = plaintext });

  fixture.io_context.poll();

  REQUIRE_FALSE(fixture.sent_bytes.empty());
  auto parsed = nlohmann::json::parse(bytes_to_string(fixture.sent_bytes));
  const std::string event_id = parsed[1]["id"].get<std::string>();

  const std::string ok_message = R"(["OK",")" + event_id + R"(",true,""])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(ok_message));

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::message_sent>(fixture.main_events[0]));
  const auto &msg_sent = std::get<radix_relay::events::message_sent>(fixture.main_events[0]);
  CHECK(msg_sent.peer == std::string(fixture.bob_rdx));
  CHECK(msg_sent.event_id == event_id);
  CHECK(msg_sent.accepted == true);
}

TEST_CASE("session_orchestrator async tracking for send command - timeout", "[session_orchestrator][async_tracking]")
{
  two_bridge_fixture fixture("/tmp/session_orch_send_timeout_alice.db", "/tmp/session_orch_send_timeout_bob.db");
  auto orchestrator = fixture.make_alice_orchestrator();

  const std::string plaintext = "Hello Bob!";
  orchestrator->handle_command(radix_relay::events::send{ .peer = std::string(fixture.bob_rdx), .message = plaintext });

  fixture.io_context.poll();
  REQUIRE_FALSE(fixture.sent_bytes.empty());

  constexpr auto timeout_wait = std::chrono::seconds(6);
  std::this_thread::sleep_for(timeout_wait);

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::message_sent>(fixture.main_events[0]));
  const auto &msg_sent = std::get<radix_relay::events::message_sent>(fixture.main_events[0]);
  CHECK(msg_sent.peer == std::string(fixture.bob_rdx));
  CHECK(msg_sent.event_id.empty());
  CHECK(msg_sent.accepted == false);
}

TEST_CASE("session_orchestrator async tracking for publish_identity - success",
  "[session_orchestrator][async_tracking]")
{
  single_bridge_fixture fixture("/tmp/session_orch_publish_ok.db");
  auto orchestrator = fixture.make_orchestrator();

  orchestrator->handle_command(radix_relay::events::publish_identity{});

  fixture.io_context.poll();

  REQUIRE_FALSE(fixture.sent_bytes.empty());
  auto parsed = nlohmann::json::parse(bytes_to_string(fixture.sent_bytes));
  const std::string event_id = parsed[1]["id"].get<std::string>();

  const std::string ok_message = R"(["OK",")" + event_id + R"(",true,""])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(ok_message));

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::bundle_published>(fixture.main_events[0]));
  const auto &bundle = std::get<radix_relay::events::bundle_published>(fixture.main_events[0]);
  CHECK(bundle.event_id == event_id);
  CHECK(bundle.accepted == true);
}

TEST_CASE("session_orchestrator async tracking for publish_identity - timeout",
  "[session_orchestrator][async_tracking]")
{
  single_bridge_fixture fixture("/tmp/session_orch_publish_timeout.db");
  auto orchestrator = fixture.make_orchestrator();

  orchestrator->handle_command(radix_relay::events::publish_identity{});

  fixture.io_context.poll();
  REQUIRE_FALSE(fixture.sent_bytes.empty());

  constexpr auto timeout_wait = std::chrono::seconds(6);
  std::this_thread::sleep_for(timeout_wait);

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::bundle_published>(fixture.main_events[0]));
  const auto &bundle = std::get<radix_relay::events::bundle_published>(fixture.main_events[0]);
  CHECK(bundle.event_id.empty());
  CHECK(bundle.accepted == false);
}

TEST_CASE("session_orchestrator async tracking for subscribe command - success",
  "[session_orchestrator][async_tracking]")
{
  single_bridge_fixture fixture("/tmp/session_orch_subscribe_ok.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string subscription_json = R"(["REQ","test_sub_123",{"kinds":[40001]}])";
  orchestrator->handle_command(radix_relay::events::subscribe{ .subscription_json = subscription_json });

  fixture.io_context.poll();

  REQUIRE_FALSE(fixture.sent_bytes.empty());
  auto sent_str = bytes_to_string(fixture.sent_bytes);
  CHECK(sent_str == subscription_json);

  const std::string eose_message = R"(["EOSE","test_sub_123"])";
  orchestrator->handle_bytes_from_transport(string_to_bytes(eose_message));

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::subscription_established>(fixture.main_events[0]));
  const auto &sub = std::get<radix_relay::events::subscription_established>(fixture.main_events[0]);
  CHECK(sub.subscription_id == "test_sub_123");
}

TEST_CASE("session_orchestrator async tracking for subscribe command - timeout",
  "[session_orchestrator][async_tracking]")
{
  single_bridge_fixture fixture("/tmp/session_orch_subscribe_timeout.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string subscription_json = R"(["REQ","test_sub_456",{"kinds":[40001]}])";
  orchestrator->handle_command(radix_relay::events::subscribe{ .subscription_json = subscription_json });

  fixture.io_context.poll();
  REQUIRE_FALSE(fixture.sent_bytes.empty());

  constexpr auto timeout_wait = std::chrono::seconds(6);
  std::this_thread::sleep_for(timeout_wait);

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::events::subscription_established>(fixture.main_events[0]));
  const auto &sub = std::get<radix_relay::events::subscription_established>(fixture.main_events[0]);
  CHECK(sub.subscription_id.empty());
}
