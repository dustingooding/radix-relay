#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <core/transport_event_display_handler.hpp>

struct transport_event_display_handler_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::display_message>> display_queue;
  radix_relay::core::transport_event_display_handler handler;

  transport_event_display_handler_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      display_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context)),
      handler(display_queue)
  {}

  [[nodiscard]] auto get_all_output() const -> std::string
  {
    std::string result;
    while (auto msg = display_queue->try_pop()) { result += msg->message; }
    return result;
  }
};

SCENARIO("Transport event display handler formats events as display messages", "[transport_event_display_handler]")
{
  GIVEN("A transport event display handler")
  {
    WHEN("handling message_received event")
    {
      constexpr std::uint64_t test_timestamp = 1234567890;
      const radix_relay::core::events::message_received evt{
        .sender_rdx = "RDX:alice123", .content = "Hello from Alice", .timestamp = test_timestamp
      };

      THEN("handler emits formatted display message")
      {
        const transport_event_display_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("RDX:alice123") != std::string::npos);
        REQUIRE(output.find("Hello from Alice") != std::string::npos);
      }
    }

    WHEN("handling session_established event")
    {
      const radix_relay::core::events::session_established evt{ .peer_rdx = "RDX:bob456" };

      THEN("handler emits session established message")
      {
        const transport_event_display_handler_fixture fixture;
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
        const transport_event_display_handler_fixture fixture;
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
        const transport_event_display_handler_fixture fixture;
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
        const transport_event_display_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("published") != std::string::npos);
      }
    }

    WHEN("handling subscription_established event")
    {
      const radix_relay::core::events::subscription_established evt{ .subscription_id = "sub123" };

      THEN("handler emits subscription message")
      {
        const transport_event_display_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("sub123") != std::string::npos);
        REQUIRE(output.find("Subscription") != std::string::npos);
      }
    }

    WHEN("handling bundle_announcement_received event")
    {
      const radix_relay::core::events::bundle_announcement_received evt{
        .pubkey = "npub123", .bundle_content = "bundle_data", .event_id = "evt456"
      };

      THEN("handler emits bundle announcement message")
      {
        const transport_event_display_handler_fixture fixture;
        fixture.handler.handle(evt);

        const auto output = fixture.get_all_output();
        REQUIRE(output.find("bundle") != std::string::npos);
      }
    }
  }
}
