#include <catch2/catch_test_macros.hpp>
#include <radix_relay/nostr_protocol.hpp>
#include <radix_relay/nostr_request_tracker.hpp>

TEST_CASE("RequestTracker track() stores pending request", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  bool callback_invoked = false;
  auto callback = [&callback_invoked](const radix_relay::nostr::protocol::ok &) { callback_invoked = true; };

  constexpr auto timeout = std::chrono::seconds(5);
  tracker.track("event_123", callback, timeout);

  CHECK(!callback_invoked);
  CHECK(tracker.has_pending("event_123"));
}

TEST_CASE("RequestTracker resolve() invokes callback with response", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  bool callback_invoked = false;
  radix_relay::nostr::protocol::ok received_response;

  auto callback = [&callback_invoked, &received_response](const radix_relay::nostr::protocol::ok &response) {
    callback_invoked = true;
    received_response = response;
  };

  constexpr auto timeout = std::chrono::seconds(5);
  tracker.track("event_456", callback, timeout);

  radix_relay::nostr::protocol::ok expected_response;
  expected_response.event_id = "event_456";
  expected_response.accepted = true;
  expected_response.message = "";

  tracker.resolve("event_456", expected_response);

  CHECK(callback_invoked);
  CHECK(received_response.event_id == expected_response.event_id);
  CHECK(received_response.accepted == expected_response.accepted);
}

TEST_CASE("RequestTracker resolve() removes request from pending", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  auto callback = [](const radix_relay::nostr::protocol::ok &) {};

  constexpr auto timeout = std::chrono::seconds(5);
  tracker.track("event_789", callback, timeout);

  CHECK(tracker.has_pending("event_789"));

  radix_relay::nostr::protocol::ok response;
  response.event_id = "event_789";
  response.accepted = true;

  tracker.resolve("event_789", response);

  CHECK(!tracker.has_pending("event_789"));
}

TEST_CASE("RequestTracker resolve() on non-existent ID does nothing", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  radix_relay::nostr::protocol::ok response;
  response.event_id = "nonexistent";
  response.accepted = false;

  tracker.resolve("nonexistent", response);

  CHECK(!tracker.has_pending("nonexistent"));
}

TEST_CASE("RequestTracker timeout invokes callback with timeout error", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  bool callback_invoked = false;
  radix_relay::nostr::protocol::ok received_response;

  auto callback = [&callback_invoked, &received_response](const radix_relay::nostr::protocol::ok &response) {
    callback_invoked = true;
    received_response = response;
  };

  constexpr auto timeout = std::chrono::milliseconds(50);
  tracker.track("event_timeout", callback, timeout);

  io_context.run();

  CHECK(callback_invoked);
  CHECK(received_response.event_id == "event_timeout");
  CHECK(!received_response.accepted);
  CHECK(received_response.message.find("timeout") != std::string::npos);
  CHECK(!tracker.has_pending("event_timeout"));
}

TEST_CASE("RequestTracker resolve() cancels timer", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::RequestTracker tracker(io_context);

  int callback_count = 0;

  auto callback = [&callback_count](const radix_relay::nostr::protocol::ok &) { ++callback_count; };

  constexpr auto timeout = std::chrono::seconds(10);
  tracker.track("event_cancel", callback, timeout);

  radix_relay::nostr::protocol::ok response;
  response.event_id = "event_cancel";
  response.accepted = true;

  tracker.resolve("event_cancel", response);

  CHECK(callback_count == 1);

  io_context.run();

  CHECK(callback_count == 1);
}
