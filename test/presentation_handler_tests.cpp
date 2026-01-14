#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <core/presentation_handler.hpp>

struct presentation_handler_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>> display_queue;
  radix_relay::core::presentation_handler handler;

  presentation_handler_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      display_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>>(
          io_context)),
      handler(radix_relay::core::presentation_handler::out_queues_t{ .display = display_queue })
  {}

  [[nodiscard]] auto get_all_output() const -> std::string
  {
    std::string result;
    while (auto msg = display_queue->try_pop()) {
      std::visit(
        [&result](const auto &evt) {
          if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
            result += evt.message;
          }
        },
        *msg);
    }
    return result;
  }
};

TEST_CASE("Presentation handler formats message_received without alias", "[presentation_handler]")
{
  constexpr std::uint64_t test_timestamp = 1234567890;
  const radix_relay::core::events::message_received evt{ .sender_rdx = "RDX:alice123",
    .sender_alias = "",
    .content = "Hello from Alice",
    .timestamp = test_timestamp,
    .should_republish_bundle = false };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("RDX:alice123") != std::string::npos);
  CHECK(output.find("Hello from Alice") != std::string::npos);
}

TEST_CASE("Presentation handler formats message_received with alias", "[presentation_handler]")
{
  constexpr std::uint64_t test_timestamp = 1234567890;
  const radix_relay::core::events::message_received evt{ .sender_rdx = "RDX:alice123",
    .sender_alias = "Alice",
    .content = "Hello from Alice",
    .timestamp = test_timestamp,
    .should_republish_bundle = false };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("Alice") != std::string::npos);
  CHECK(output.find("RDX:alice123") == std::string::npos);
  CHECK(output.find("Hello from Alice") != std::string::npos);
}

TEST_CASE("Presentation handler formats session_established event", "[presentation_handler]")
{
  const radix_relay::core::events::session_established evt{ .peer_rdx = "RDX:bob456" };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("RDX:bob456") != std::string::npos);
  CHECK(output.find("session") != std::string::npos);
}

TEST_CASE("Presentation handler formats message_sent with accepted=true", "[presentation_handler]")
{
  const radix_relay::core::events::message_sent evt{ .peer = "alice", .event_id = "evt123", .accepted = true };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("alice") != std::string::npos);
  CHECK(output.find("sent") != std::string::npos);
}

TEST_CASE("Presentation handler formats message_sent with accepted=false", "[presentation_handler]")
{
  const radix_relay::core::events::message_sent evt{ .peer = "alice", .event_id = "", .accepted = false };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("Failed") != std::string::npos);
}

TEST_CASE("Presentation handler formats bundle_published event", "[presentation_handler]")
{
  const radix_relay::core::events::bundle_published evt{ .event_id = "bundle123", .accepted = true };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("published") != std::string::npos);
}

TEST_CASE("Presentation handler logs subscription_established via spdlog only", "[presentation_handler]")
{
  const radix_relay::core::events::subscription_established evt{ .subscription_id = "sub123" };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.empty());
}

TEST_CASE("Presentation handler logs bundle_announcement_received via spdlog only", "[presentation_handler]")
{
  const radix_relay::core::events::bundle_announcement_received evt{
    .pubkey = "npub123", .bundle_content = "bundle_data", .event_id = "evt456"
  };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.empty());
}

TEST_CASE("Presentation handler formats identities_listed with no identities", "[presentation_handler]")
{
  const radix_relay::core::events::identities_listed evt{ .identities = {} };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("No identities") != std::string::npos);
}

TEST_CASE("Presentation handler formats identities_listed with identities", "[presentation_handler]")
{
  std::vector<radix_relay::core::events::discovered_identity> identities;
  identities.push_back(radix_relay::core::events::discovered_identity{
    .rdx_fingerprint = "RDX:abc123", .nostr_pubkey = "npub_alice", .event_id = "evt_alice" });
  identities.push_back(radix_relay::core::events::discovered_identity{
    .rdx_fingerprint = "RDX:def456", .nostr_pubkey = "npub_bob", .event_id = "evt_bob" });

  const radix_relay::core::events::identities_listed evt{ .identities = identities };

  const presentation_handler_fixture fixture;
  fixture.handler.handle(evt);

  const auto output = fixture.get_all_output();
  CHECK(output.find("Discovered identities") != std::string::npos);
  CHECK(output.find("RDX:abc123") != std::string::npos);
  CHECK(output.find("npub_alice") != std::string::npos);
  CHECK(output.find("RDX:def456") != std::string::npos);
  CHECK(output.find("npub_bob") != std::string::npos);
}
