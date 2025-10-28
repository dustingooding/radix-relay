#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <radix_relay/core/session_orchestrator.hpp>
#include <radix_relay/nostr/request_tracker.hpp>
#include <radix_relay/signal/node_identity.hpp>
#include <radix_relay/signal/signal_bridge.hpp>
#include <ranges>
#include <signal_bridge_cxx/lib.h>
#include <spdlog/spdlog.h>

namespace {

struct orchestrator_test_fixture
{
  boost::asio::io_context io_context;
  boost::asio::io_context::strand main_strand{ io_context };
  boost::asio::io_context::strand session_strand{ io_context };
  boost::asio::io_context::strand transport_strand{ io_context };
  std::shared_ptr<radix_relay::nostr::request_tracker> tracker{ std::make_shared<radix_relay::nostr::request_tracker>(
    &io_context) };
  mutable std::vector<radix_relay::core::events::transport_event_variant_t> main_events;

  auto make_send_event_to_main() const -> std::function<void(radix_relay::core::events::transport_event_variant_t)>
  {
    return [this](radix_relay::core::events::transport_event_variant_t evt) { main_events.push_back(std::move(evt)); };
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
  str.resize(bytes.size());
  std::ranges::transform(bytes, str.begin(), [](std::byte byte) { return std::bit_cast<char>(byte); });
  return str;
}

class two_bridge_fixture : public orchestrator_test_fixture
{
public:
  two_bridge_fixture(std::string alice_path, std::string bob_path)
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
  }

  ~two_bridge_fixture() noexcept
  {
    std::error_code error_code;
    std::filesystem::remove(alice_db_path, error_code);
    std::filesystem::remove(bob_db_path, error_code);
  }

  two_bridge_fixture(const two_bridge_fixture &) = delete;
  auto operator=(const two_bridge_fixture &) -> two_bridge_fixture & = delete;
  two_bridge_fixture(two_bridge_fixture &&) = delete;
  auto operator=(two_bridge_fixture &&) -> two_bridge_fixture & = delete;

  [[nodiscard]] auto get_alice_bridge() const -> std::shared_ptr<radix_relay::signal::bridge> { return alice_bridge; }
  [[nodiscard]] auto get_bob_bridge() const -> std::shared_ptr<radix_relay::signal::bridge> { return bob_bridge; }

  std::string alice_db_path;
  std::string bob_db_path;
  std::shared_ptr<radix_relay::signal::bridge> alice_bridge;
  std::shared_ptr<radix_relay::signal::bridge> bob_bridge;
  std::string alice_rdx;
  std::string bob_rdx;
};

struct event_driven_fixture : orchestrator_test_fixture
{
  std::string db_path;
  std::shared_ptr<radix_relay::signal::bridge> bridge;
  mutable std::vector<radix_relay::core::events::transport::command_variant_t> transport_commands;

  explicit event_driven_fixture(std::string path) : db_path(std::move(path))
  {
    std::filesystem::remove(db_path);
    bridge = std::make_shared<radix_relay::signal::bridge>(db_path);
  }

  ~event_driven_fixture() noexcept
  {
    std::error_code error_code;
    std::filesystem::remove(db_path, error_code);
  }

  event_driven_fixture(const event_driven_fixture &) = delete;
  auto operator=(const event_driven_fixture &) -> event_driven_fixture & = delete;
  event_driven_fixture(event_driven_fixture &&) = delete;
  auto operator=(event_driven_fixture &&) -> event_driven_fixture & = delete;

  auto make_send_transport_command() const
    -> std::function<void(radix_relay::core::events::transport::command_variant_t)>
  {
    return [this](radix_relay::core::events::transport::command_variant_t cmd) {
      transport_commands.push_back(std::move(cmd));
    };
  }

  auto make_orchestrator() const -> std::shared_ptr<
    radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>
  {
    if (not bridge) { throw std::runtime_error("Bridge not initialized"); }
    return std::make_shared<
      radix_relay::core::session_orchestrator<radix_relay::signal::bridge, radix_relay::nostr::request_tracker>>(bridge,
      tracker,
      radix_relay::core::strands{ .main = &main_strand, .session = &session_strand, .transport = &transport_strand },
      make_send_transport_command(),
      make_send_event_to_main());
  }
};

}// namespace

TEST_CASE("session_orchestrator event-driven sends transport::send command", "[session_orchestrator][event_driven]")
{
  const two_bridge_fixture fixture("/tmp/session_orch_event_send_alice.db", "/tmp/session_orch_event_send_bob.db");

  event_driven_fixture event_fixture("/tmp/session_orch_event_send.db");
  event_fixture.bridge = fixture.get_alice_bridge();

  auto orchestrator = event_fixture.make_orchestrator();

  orchestrator->handle_command(
    radix_relay::core::events::send{ .peer = fixture.bob_rdx, .message = "Hello via events!" });
  event_fixture.io_context.poll();

  REQUIRE(event_fixture.transport_commands.size() == 1);
  REQUIRE(std::holds_alternative<radix_relay::core::events::transport::send>(event_fixture.transport_commands[0]));

  const auto &send_cmd = std::get<radix_relay::core::events::transport::send>(event_fixture.transport_commands[0]);
  CHECK_FALSE(send_cmd.message_id.empty());
  CHECK_FALSE(send_cmd.bytes.empty());

  auto sent_str = bytes_to_string(send_cmd.bytes);
  CHECK(sent_str.starts_with("[\"EVENT\","));
}

TEST_CASE("session_orchestrator event-driven receives bytes via transport::bytes_received event",
  "[session_orchestrator][event_driven]")
{
  const two_bridge_fixture fixture("/tmp/session_orch_event_recv_alice.db", "/tmp/session_orch_event_recv_bob.db");

  event_driven_fixture event_fixture("/tmp/session_orch_event_recv.db");
  event_fixture.bridge = fixture.get_alice_bridge();

  auto orchestrator = event_fixture.make_orchestrator();

  orchestrator->handle_command(
    radix_relay::core::events::send{ .peer = fixture.bob_rdx, .message = "Request session" });
  event_fixture.io_context.poll();

  REQUIRE(event_fixture.transport_commands.size() == 1);

  auto parsed = nlohmann::json::parse(
    bytes_to_string(std::get<radix_relay::core::events::transport::send>(event_fixture.transport_commands[0]).bytes));
  const std::string event_id = parsed[1]["id"].get<std::string>();

  const std::string ok_message = R"(["OK",")" + event_id + R"(",true,""])";
  const radix_relay::core::events::transport::bytes_received recv_evt{ .bytes = string_to_bytes(ok_message) };

  boost::asio::post(event_fixture.session_strand, [orchestrator, recv_evt]() {// NOLINT(bugprone-exception-escape)
    try {
      orchestrator->handle_event(recv_evt);
    } catch (const std::exception &e) {
      spdlog::error("[test] handle_event failed: {}", e.what());
    }
  });

  event_fixture.io_context.run();

  REQUIRE(event_fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::core::events::message_sent>(event_fixture.main_events[0]));
  const auto &msg_sent = std::get<radix_relay::core::events::message_sent>(event_fixture.main_events[0]);
  CHECK(msg_sent.event_id == event_id);
  CHECK(msg_sent.accepted);
}

TEST_CASE("session_orchestrator event-driven publish_identity sends transport command",
  "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_publish.db");
  auto orchestrator = fixture.make_orchestrator();

  orchestrator->handle_command(radix_relay::core::events::publish_identity{});
  fixture.io_context.poll();

  REQUIRE(fixture.transport_commands.size() == 1);
  REQUIRE(std::holds_alternative<radix_relay::core::events::transport::send>(fixture.transport_commands[0]));

  const auto &send_cmd = std::get<radix_relay::core::events::transport::send>(fixture.transport_commands[0]);
  CHECK_FALSE(send_cmd.message_id.empty());
  CHECK_FALSE(send_cmd.bytes.empty());
}

TEST_CASE("session_orchestrator event-driven subscribe sends transport command", "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_subscribe.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string subscription_json = R"(["REQ","test_sub_789",{"kinds":[40001]}])";
  orchestrator->handle_command(radix_relay::core::events::subscribe{ .subscription_json = subscription_json });

  fixture.io_context.poll();

  REQUIRE(fixture.transport_commands.size() == 1);
  REQUIRE(std::holds_alternative<radix_relay::core::events::transport::send>(fixture.transport_commands[0]));

  const auto &send_cmd = std::get<radix_relay::core::events::transport::send>(fixture.transport_commands[0]);
  CHECK_FALSE(send_cmd.message_id.empty());
  CHECK(bytes_to_string(send_cmd.bytes) == subscription_json);
}

TEST_CASE("session_orchestrator event-driven handles incoming encrypted_message",
  "[session_orchestrator][event_driven]")
{
  two_bridge_fixture fixture("/tmp/session_orch_event_enc_msg_alice.db", "/tmp/session_orch_event_enc_msg_bob.db");

  event_driven_fixture event_fixture("/tmp/session_orch_event_enc_msg.db");
  event_fixture.bridge = fixture.bob_bridge;

  auto orchestrator = event_fixture.make_orchestrator();

  const std::string plaintext = "Hello Bob via events!";
  const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

  auto encrypted_bytes = fixture.alice_bridge->encrypt_message(fixture.bob_rdx, message_bytes);

  std::string hex_content;
  for (const auto &byte : encrypted_bytes) { hex_content += std::format("{:02x}", byte); }

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t encrypted_message_kind = 40001;
  const nlohmann::json event_json = { { "id", "test_event_id_encrypted" },
    { "pubkey", fixture.alice_rdx },
    { "created_at", test_timestamp },
    { "kind", encrypted_message_kind },
    { "content", hex_content },
    { "sig", "signature" },
    { "tags", nlohmann::json::array({ nlohmann::json::array({ "p", fixture.alice_rdx }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "sub_id", event_json }).dump();
  const radix_relay::core::events::transport::bytes_received recv_evt{ .bytes = string_to_bytes(nostr_message_json) };

  boost::asio::post(event_fixture.session_strand, [orchestrator, recv_evt]() {// NOLINT(bugprone-exception-escape)
    try {
      orchestrator->handle_event(recv_evt);
    } catch (const std::exception &e) {
      spdlog::error("[test] handle_event failed: {}", e.what());
    }
  });

  event_fixture.io_context.run();

  REQUIRE(event_fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::core::events::message_received>(event_fixture.main_events[0]));

  const auto &msg = std::get<radix_relay::core::events::message_received>(event_fixture.main_events[0]);
  CHECK(msg.sender_rdx == fixture.alice_rdx);
  CHECK(msg.content == plaintext);
}

TEST_CASE("session_orchestrator event-driven handles incoming bundle_announcement",
  "[session_orchestrator][event_driven]")
{
  event_driven_fixture bob_fixture("/tmp/session_orch_event_bundle_bob.db");
  auto bob_orchestrator = bob_fixture.make_orchestrator();

  const event_driven_fixture alice_fixture("/tmp/session_orch_event_bundle_alice.db");

  const auto alice_bundle_json_str = alice_fixture.bridge->generate_prekey_bundle_announcement("test-0.1.0");
  const auto alice_bundle_json = nlohmann::json::parse(alice_bundle_json_str);
  const std::string alice_bundle_base64 = alice_bundle_json["content"].template get<std::string>();

  const auto alice_rdx_tag = alice_bundle_json["tags"][1][1].template get<std::string>();

  constexpr std::uint64_t test_timestamp = 1234567890;
  constexpr std::uint32_t bundle_announcement_kind = 30078;
  const nlohmann::json event_json = { { "id", "test_bundle_event_id" },
    { "pubkey", "alice_nostr_pubkey" },
    { "created_at", test_timestamp },
    { "kind", bundle_announcement_kind },
    { "content", alice_bundle_base64 },
    { "sig", "signature" },
    { "tags",
      nlohmann::json::array({ nlohmann::json::array({ "d", "radix_prekey_bundle_v1" }),
        nlohmann::json::array({ "rdx", alice_rdx_tag }) }) } };

  const std::string nostr_message_json = nlohmann::json::array({ "EVENT", "bundle_sub", event_json }).dump();
  const radix_relay::core::events::transport::bytes_received recv_evt{ .bytes = string_to_bytes(nostr_message_json) };

  boost::asio::post(bob_fixture.session_strand, [bob_orchestrator, recv_evt]() {// NOLINT(bugprone-exception-escape)
    try {
      bob_orchestrator->handle_event(recv_evt);
    } catch (const std::exception &e) {
      spdlog::error("[test] handle_event failed: {}", e.what());
    }
  });

  bob_fixture.io_context.run();

  REQUIRE(bob_fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::core::events::bundle_announcement_received>(bob_fixture.main_events[0]));

  const auto &bundle = std::get<radix_relay::core::events::bundle_announcement_received>(bob_fixture.main_events[0]);
  CHECK(bundle.pubkey == "alice_nostr_pubkey");
  CHECK(bundle.bundle_content == alice_bundle_base64);
  CHECK(bundle.event_id == "test_bundle_event_id");
}

TEST_CASE("session_orchestrator event-driven handles malformed JSON", "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_malformed.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string malformed_json = "{ this is not valid json }";
  const radix_relay::core::events::transport::bytes_received recv_evt{ .bytes = string_to_bytes(malformed_json) };

  boost::asio::post(fixture.session_strand, [orchestrator, recv_evt]() {// NOLINT(bugprone-exception-escape)
    try {
      orchestrator->handle_event(recv_evt);
    } catch (const std::exception &e) {
      spdlog::error("[test] handle_event failed: {}", e.what());
    }
  });

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator event-driven handles unknown protocol", "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_unknown_proto.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string unknown_protocol_message = R"(["UNKNOWN_MESSAGE_TYPE","param1","param2"])";
  const radix_relay::core::events::transport::bytes_received recv_evt{ .bytes =
                                                                         string_to_bytes(unknown_protocol_message) };

  boost::asio::post(fixture.session_strand, [orchestrator, recv_evt]() {// NOLINT(bugprone-exception-escape)
    try {
      orchestrator->handle_event(recv_evt);
    } catch (const std::exception &e) {
      spdlog::error("[test] handle_event failed: {}", e.what());
    }
  });

  fixture.io_context.run();

  CHECK(fixture.main_events.empty());
}

TEST_CASE("session_orchestrator event-driven async tracking timeout for publish_identity",
  "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_publish_timeout.db");
  auto orchestrator = fixture.make_orchestrator();

  orchestrator->handle_command(radix_relay::core::events::publish_identity{});
  fixture.io_context.poll();

  REQUIRE(fixture.transport_commands.size() == 1);

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::core::events::bundle_published>(fixture.main_events[0]));

  const auto &pub = std::get<radix_relay::core::events::bundle_published>(fixture.main_events[0]);
  CHECK(pub.event_id.empty());
  CHECK_FALSE(pub.accepted);
}

TEST_CASE("session_orchestrator event-driven async tracking timeout for subscribe",
  "[session_orchestrator][event_driven]")
{
  event_driven_fixture fixture("/tmp/session_orch_event_subscribe_timeout.db");
  auto orchestrator = fixture.make_orchestrator();

  const std::string subscription_json = R"(["REQ","timeout_sub",{"kinds":[40001]}])";
  orchestrator->handle_command(radix_relay::core::events::subscribe{ .subscription_json = subscription_json });

  fixture.io_context.poll();

  REQUIRE(fixture.transport_commands.size() == 1);

  fixture.io_context.run();

  REQUIRE(fixture.main_events.size() == 1);
  CHECK(std::holds_alternative<radix_relay::core::events::subscription_established>(fixture.main_events[0]));

  const auto &sub = std::get<radix_relay::core::events::subscription_established>(fixture.main_events[0]);
  CHECK(sub.subscription_id.empty());
}
