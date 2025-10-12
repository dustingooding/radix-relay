#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

#include <radix_relay/events/events.hpp>

SCENARIO("Event structures can be constructed and hold data correctly", "[events][construction]")
{
  GIVEN("Simple events can be constructed")
  {
    WHEN("constructing simple events")
    {
      std::ignore = radix_relay::events::help{};
      std::ignore = radix_relay::events::peers{};
      std::ignore = radix_relay::events::status{};
      std::ignore = radix_relay::events::sessions{};
      std::ignore = radix_relay::events::scan{};
      std::ignore = radix_relay::events::version{};

      THEN("they should be constructed successfully") { REQUIRE(true); }
    }
  }

  GIVEN("Parameterized events can be constructed with data")
  {
    WHEN("constructing mode event")
    {
      auto mode_event = radix_relay::events::mode{ "internet" };

      THEN("it should hold the mode data") { REQUIRE(mode_event.new_mode == "internet"); }
    }

    WHEN("constructing send event")
    {
      auto send_event = radix_relay::events::send{ .peer = "alice", .message = "hello world" };

      THEN("it should hold peer and message data")
      {
        REQUIRE(send_event.peer == "alice");
        REQUIRE(send_event.message == "hello world");
      }
    }

    WHEN("constructing broadcast event")
    {
      auto broadcast_event = radix_relay::events::broadcast{ "hello everyone" };

      THEN("it should hold the message data") { REQUIRE(broadcast_event.message == "hello everyone"); }
    }

    WHEN("constructing connect event")
    {
      auto connect_event = radix_relay::events::connect{ "wss://relay.damus.io" };

      THEN("it should hold the relay data") { REQUIRE(connect_event.relay == "wss://relay.damus.io"); }
    }

    WHEN("constructing trust event")
    {
      auto trust_event = radix_relay::events::trust{ .peer = "alice", .alias = "" };

      THEN("it should hold the peer data") { REQUIRE(trust_event.peer == "alice"); }
    }

    WHEN("constructing verify event")
    {
      auto verify_event = radix_relay::events::verify{ "bob" };

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
        REQUIRE(radix_relay::events::Event<radix_relay::events::help>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::peers>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::status>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::sessions>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::scan>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::version>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::mode>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::send>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::broadcast>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::connect>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::trust>);
        REQUIRE(radix_relay::events::Event<radix_relay::events::verify>);
      }

      AND_THEN("non-event types should not satisfy Event concept")
      {
        REQUIRE_FALSE(radix_relay::events::Event<int>);
        REQUIRE_FALSE(radix_relay::events::Event<std::string>);
      }
    }
  }
}
