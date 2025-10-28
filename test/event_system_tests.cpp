#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

#include <radix_relay/core/events.hpp>

SCENARIO("Event structures can be constructed and hold data correctly", "[events][construction]")
{
  GIVEN("Simple events can be constructed")
  {
    WHEN("constructing simple events")
    {
      std::ignore = radix_relay::core::events::help{};
      std::ignore = radix_relay::core::events::peers{};
      std::ignore = radix_relay::core::events::status{};
      std::ignore = radix_relay::core::events::sessions{};
      std::ignore = radix_relay::core::events::scan{};
      std::ignore = radix_relay::core::events::version{};

      THEN("they should be constructed successfully") { REQUIRE(true); }
    }
  }

  GIVEN("Parameterized events can be constructed with data")
  {
    WHEN("constructing mode event")
    {
      auto mode_event = radix_relay::core::events::mode{ "internet" };

      THEN("it should hold the mode data") { REQUIRE(mode_event.new_mode == "internet"); }
    }

    WHEN("constructing send event")
    {
      auto send_event = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };

      THEN("it should hold peer and message data")
      {
        REQUIRE(send_event.peer == "alice");
        REQUIRE(send_event.message == "hello world");
      }
    }

    WHEN("constructing broadcast event")
    {
      auto broadcast_event = radix_relay::core::events::broadcast{ "hello everyone" };

      THEN("it should hold the message data") { REQUIRE(broadcast_event.message == "hello everyone"); }
    }

    WHEN("constructing connect event")
    {
      auto connect_event = radix_relay::core::events::connect{ "wss://relay.damus.io" };

      THEN("it should hold the relay data") { REQUIRE(connect_event.relay == "wss://relay.damus.io"); }
    }

    WHEN("constructing trust event")
    {
      auto trust_event = radix_relay::core::events::trust{ .peer = "alice", .alias = "" };

      THEN("it should hold the peer data") { REQUIRE(trust_event.peer == "alice"); }
    }

    WHEN("constructing verify event")
    {
      auto verify_event = radix_relay::core::events::verify{ "bob" };

      THEN("it should hold the peer data") { REQUIRE(verify_event.peer == "bob"); }
    }
  }
}

SCENARIO("Event concept correctly identifies event types", "[events][concepts]")
{
  GIVEN("All event types")
  {
    WHEN("checking if types satisfy the Event concept")
    {
      THEN("all event types should satisfy Event concept")
      {
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::help>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::peers>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::status>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::sessions>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::scan>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::version>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::mode>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::send>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::broadcast>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::connect>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::trust>);
        REQUIRE(radix_relay::core::events::Event<radix_relay::core::events::verify>);
      }

      AND_THEN("non-event types should not satisfy Event concept")
      {
        REQUIRE_FALSE(radix_relay::core::events::Event<int>);
        REQUIRE_FALSE(radix_relay::core::events::Event<std::string>);
      }
    }
  }
}
