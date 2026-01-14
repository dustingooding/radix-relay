#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

#include <core/events.hpp>

TEST_CASE("Simple events can be constructed", "[events][construction]")
{
  std::ignore = radix_relay::core::events::help{};
  std::ignore = radix_relay::core::events::peers{};
  std::ignore = radix_relay::core::events::status{};
  std::ignore = radix_relay::core::events::sessions{};
  std::ignore = radix_relay::core::events::scan{};
  std::ignore = radix_relay::core::events::version{};

  CHECK(true);
}

TEST_CASE("Mode event holds mode data", "[events][construction]")
{
  auto mode_event = radix_relay::core::events::mode{ "internet" };

  CHECK(mode_event.new_mode == "internet");
}

TEST_CASE("Send event holds peer and message data", "[events][construction]")
{
  auto send_event = radix_relay::core::events::send{ .peer = "alice", .message = "hello world" };

  CHECK(send_event.peer == "alice");
  CHECK(send_event.message == "hello world");
}

TEST_CASE("Broadcast event holds message data", "[events][construction]")
{
  auto broadcast_event = radix_relay::core::events::broadcast{ "hello everyone" };

  CHECK(broadcast_event.message == "hello everyone");
}

TEST_CASE("Connect event holds relay data", "[events][construction]")
{
  auto connect_event = radix_relay::core::events::connect{ "wss://relay.damus.io" };

  CHECK(connect_event.relay == "wss://relay.damus.io");
}

TEST_CASE("Trust event holds peer data", "[events][construction]")
{
  auto trust_event = radix_relay::core::events::trust{ .peer = "alice", .alias = "" };

  CHECK(trust_event.peer == "alice");
}

TEST_CASE("Verify event holds peer data", "[events][construction]")
{
  auto verify_event = radix_relay::core::events::verify{ "bob" };

  CHECK(verify_event.peer == "bob");
}

TEST_CASE("All event types satisfy Event concept", "[events][concepts]")
{
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::help>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::peers>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::status>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::sessions>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::scan>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::version>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::mode>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::send>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::broadcast>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::connect>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::trust>);
  CHECK(radix_relay::core::events::Event<radix_relay::core::events::verify>);
}

TEST_CASE("Non-event types do not satisfy Event concept", "[events][concepts]")
{
  CHECK_FALSE(radix_relay::core::events::Event<int>);
  CHECK_FALSE(radix_relay::core::events::Event<std::string>);
}
