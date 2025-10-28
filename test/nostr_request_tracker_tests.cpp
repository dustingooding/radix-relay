#include <catch2/catch_test_macros.hpp>
#include <radix_relay/nostr/protocol.hpp>
#include <radix_relay/nostr/request_tracker.hpp>

TEST_CASE("request_tracker track() stores pending request", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

  bool callback_invoked = false;
  auto callback = [&callback_invoked](const radix_relay::nostr::protocol::ok &) { callback_invoked = true; };

  constexpr auto timeout = std::chrono::seconds(5);
  tracker.track("event_123", callback, timeout);

  CHECK(!callback_invoked);
  CHECK(tracker.has_pending("event_123"));
}

TEST_CASE("request_tracker resolve() invokes callback with response", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

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

TEST_CASE("request_tracker resolve() removes request from pending", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

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

TEST_CASE("request_tracker resolve() on non-existent ID does nothing", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

  radix_relay::nostr::protocol::ok response;
  response.event_id = "nonexistent";
  response.accepted = false;

  tracker.resolve("nonexistent", response);

  CHECK(!tracker.has_pending("nonexistent"));
}

TEST_CASE("request_tracker timeout invokes callback with timeout error", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

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

TEST_CASE("request_tracker resolve() cancels timer", "[nostr][request_tracker]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

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

TEST_CASE("request_tracker async_track() returns OK when response arrives", "[nostr][request_tracker][awaitable]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

  auto result = std::make_shared<radix_relay::nostr::protocol::ok>();
  auto completed = std::make_shared<bool>(false);

  constexpr auto timeout = std::chrono::seconds(5);

  boost::asio::co_spawn(
    io_context,
    [](std::reference_wrapper<radix_relay::nostr::request_tracker> tracker_ref,
      std::shared_ptr<radix_relay::nostr::protocol::ok> result_ptr,
      std::shared_ptr<bool> completed_ptr,
      std::chrono::milliseconds timeout_val) -> boost::asio::awaitable<void> {
      *result_ptr = co_await tracker_ref.get().async_track("event_async", timeout_val);
      *completed_ptr = true;
    }(std::ref(tracker), result, completed, timeout),
    boost::asio::detached);

  boost::asio::post(io_context, [&tracker]() {
    radix_relay::nostr::protocol::ok response;
    response.event_id = "event_async";
    response.accepted = true;
    response.message = "OK";

    tracker.resolve("event_async", response);
  });

  io_context.run();

  CHECK(*completed);
  CHECK(result->event_id == "event_async");
  CHECK(result->accepted);
  CHECK(result->message == "OK");
}

TEST_CASE("request_tracker async_track() throws on timeout", "[nostr][request_tracker][awaitable]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

  auto exception_thrown = std::make_shared<bool>(false);
  auto exception_message = std::make_shared<std::string>();

  constexpr auto timeout = std::chrono::milliseconds(50);

  boost::asio::co_spawn(
    io_context,
    [](std::reference_wrapper<radix_relay::nostr::request_tracker> tracker_ref,
      std::shared_ptr<bool> thrown_ptr,
      std::shared_ptr<std::string> message_ptr,
      std::chrono::milliseconds timeout_val) -> boost::asio::awaitable<void> {
      try {
        co_await tracker_ref.get().async_track("event_timeout", timeout_val);
      } catch (const std::exception &e) {
        *thrown_ptr = true;
        *message_ptr = e.what();
      }
    }(std::ref(tracker), exception_thrown, exception_message, timeout),
    boost::asio::detached);

  io_context.run();

  CHECK(*exception_thrown);
  CHECK(exception_message->find("timeout") != std::string::npos);
}

TEST_CASE("request_tracker async_track() works with EOSE responses", "[nostr][request_tracker][awaitable][eose]")
{
  boost::asio::io_context io_context;
  radix_relay::nostr::request_tracker tracker(&io_context);

  auto result = std::make_shared<radix_relay::nostr::protocol::eose>();
  auto completed = std::make_shared<bool>(false);

  constexpr auto timeout = std::chrono::seconds(5);

  boost::asio::co_spawn(
    io_context,
    [](std::reference_wrapper<radix_relay::nostr::request_tracker> tracker_ref,
      std::shared_ptr<radix_relay::nostr::protocol::eose> result_ptr,
      std::shared_ptr<bool> completed_ptr,
      std::chrono::milliseconds timeout_val) -> boost::asio::awaitable<void> {
      *result_ptr = co_await tracker_ref.get().async_track<radix_relay::nostr::protocol::eose>("sub_123", timeout_val);
      *completed_ptr = true;
    }(std::ref(tracker), result, completed, timeout),
    boost::asio::detached);

  boost::asio::post(io_context, [&tracker]() {
    radix_relay::nostr::protocol::eose response;
    response.subscription_id = "sub_123";
    tracker.resolve("sub_123", response);
  });

  io_context.run();

  CHECK(*completed);
  CHECK(result->subscription_id == "sub_123");
}
