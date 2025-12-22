#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <core/presentation_handler.hpp>

struct presentation_handler_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::display_message>> display_queue;
  radix_relay::core::presentation_handler handler;

  presentation_handler_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      display_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context)),
      handler(radix_relay::core::presentation_handler::out_queues_t{ .display = display_queue })
  {}

  [[nodiscard]] auto get_all_output() const -> std::string
  {
    std::string result;
    while (auto msg = display_queue->try_pop()) { result += msg->message; }
    return result;
  }
};

SCENARIO("Transport event display handler formats events as display messages", "[presentation_handler]")
{
  GIVEN("A transport event display handler")
  {
    WHEN("handling message_received event without alias")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      const radix_relay::core::events::message_received evt{ .sender_rdx = "RDX:alice123",
        .sender_alias = "",
        .content = "Hello from Alice",
        .timestamp = test_timestamp,
        .should_republish_bundle = false };

      THEN("handler emits formatted display message with RDX fingerprint")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("RDX:alice123") != std::string::npos);
        REQUIRE(output.find("Hello from Alice") != std::string::npos);
      }
    }

    WHEN("handling message_received event with alias")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      const radix_relay::core::events::message_received evt{ .sender_rdx = "RDX:alice123",
        .sender_alias = "Alice",
        .content = "Hello from Alice",
        .timestamp = test_timestamp,
        .should_republish_bundle = false };

      THEN("handler emits formatted display message with alias")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("Alice") != std::string::npos);
        REQUIRE(output.find("RDX:alice123") == std::string::npos);
        REQUIRE(output.find("Hello from Alice") != std::string::npos);
      }
    }

    WHEN("handling session_established event")
    {
      const radix_relay::core::events::session_established evt{ .peer_rdx = "RDX:bob456" };

      THEN("handler emits session established message")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("RDX:bob456") != std::string::npos);
        REQUIRE(output.find("session") != std::string::npos);
      }
    }

    WHEN("handling message_sent event with accepted=true")
    {
      const radix_relay::core::events::message_sent evt{ .peer = "alice", .event_id = "evt123", .accepted = true };

      THEN("handler emits success message")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("alice") != std::string::npos);
        REQUIRE(output.find("sent") != std::string::npos);
      }
    }

    WHEN("handling message_sent event with accepted=false")
    {
      const radix_relay::core::events::message_sent evt{ .peer = "alice", .event_id = "", .accepted = false };

      THEN("handler emits failure message")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("Failed") != std::string::npos);
      }
    }

    WHEN("handling bundle_published event")
    {
      const radix_relay::core::events::bundle_published evt{ .event_id = "bundle123", .accepted = true };

      THEN("handler emits bundle published message")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("published") != std::string::npos);
      }
    }

    WHEN("handling subscription_established event")
    {
      const radix_relay::core::events::subscription_established evt{ .subscription_id = "sub123" };

      THEN("handler logs subscription via spdlog (not display queue)")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.empty());
      }
    }

    WHEN("handling bundle_announcement_received event")
    {
      const radix_relay::core::events::bundle_announcement_received evt{
        .pubkey = "npub123", .bundle_content = "bundle_data", .event_id = "evt456"
      };

      THEN("handler logs bundle announcement via spdlog (not display queue)")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.empty());
      }
    }

    WHEN("handling identities_listed event with no identities")
    {
      const radix_relay::core::events::identities_listed evt{ .identities = {} };

      THEN("handler emits no identities message")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("No identities") != std::string::npos);
      }
    }

    WHEN("handling identities_listed event with identities")
    {
      std::vector<radix_relay::core::events::discovered_identity> identities;
      identities.push_back(radix_relay::core::events::discovered_identity{
        .rdx_fingerprint = "RDX:abc123", .nostr_pubkey = "npub_alice", .event_id = "evt_alice" });
      identities.push_back(radix_relay::core::events::discovered_identity{
        .rdx_fingerprint = "RDX:def456", .nostr_pubkey = "npub_bob", .event_id = "evt_bob" });

      const radix_relay::core::events::identities_listed evt{ .identities = identities };

      THEN("handler emits formatted list of identities")
      {
        const presentation_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("Discovered identities") != std::string::npos);
        REQUIRE(output.find("RDX:abc123") != std::string::npos);
        REQUIRE(output.find("npub_alice") != std::string::npos);
        REQUIRE(output.find("RDX:def456") != std::string::npos);
        REQUIRE(output.find("npub_bob") != std::string::npos);
      }
    }
  }
}
