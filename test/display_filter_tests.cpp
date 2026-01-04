#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/display_filter.hpp>
#include <core/events.hpp>
#include <core/standard_processor.hpp>
#include <memory>
#include <vector>

using namespace radix_relay;

namespace {

// Test timestamp constants to avoid magic numbers
constexpr std::uint64_t TIMESTAMP_1 = 100;
constexpr std::uint64_t TIMESTAMP_2 = 200;
constexpr std::uint64_t TIMESTAMP_3 = 300;
constexpr std::uint64_t TIMESTAMP_4 = 400;

// Helper to create display messages with specific properties
auto make_message(std::string content,
  core::events::display_message::source source,
  std::optional<std::string> contact = std::nullopt,
  std::uint64_t timestamp = 0) -> core::events::display_message
{
  return core::events::display_message{
    .message = std::move(content), .contact_rdx = std::move(contact), .timestamp = timestamp, .source_type = source
  };
}

}// namespace

TEST_CASE("display_filter passes all messages when not in chat mode", "[display_filter]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();

  auto filtered_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);

  const core::display_filter filter(core::display_filter::out_queues_t{ .filtered = filtered_queue });

  SECTION("system messages pass through")
  {
    auto msg = make_message("System message", core::events::display_message::source::system);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "System message"); }
  }

  SECTION("incoming messages pass through")
  {
    auto msg = make_message("Hello", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->message == "Hello");
      CHECK(result->contact_rdx == "RDX:alice");
    }
  }

  SECTION("outgoing messages pass through")
  {
    auto msg =
      make_message("Hi there", core::events::display_message::source::outgoing_message, "RDX:bob", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) {
      CHECK(result->message == "Hi there");
      CHECK(result->contact_rdx == "RDX:bob");
    }
  }

  SECTION("command feedback passes through")
  {
    auto msg = make_message("Command executed", core::events::display_message::source::command_feedback);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "Command executed"); }
  }
}

TEST_CASE("display_filter system messages always pass through", "[display_filter]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();

  auto filtered_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);
  const core::display_filter filter(core::display_filter::out_queues_t{ .filtered = filtered_queue });

  filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });

  SECTION("system message passes even in chat mode")
  {
    auto msg = make_message("System alert", core::events::display_message::source::system);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "System alert"); }
  }

  SECTION("system message with different contact still passes")
  {
    auto msg = make_message("System info", core::events::display_message::source::system, "RDX:bob");
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "System info"); }
  }
}

TEST_CASE("display_filter filters messages in chat mode", "[display_filter]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();

  auto filtered_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);
  const core::display_filter filter(core::display_filter::out_queues_t{ .filtered = filtered_queue });

  filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });

  SECTION("messages from active contact pass through")
  {
    auto msg = make_message(
      "Hello from Alice", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "Hello from Alice"); }
  }

  SECTION("messages to active contact pass through")
  {
    auto msg =
      make_message("Reply to Alice", core::events::display_message::source::outgoing_message, "RDX:alice", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    REQUIRE(result.has_value());
    if (result.has_value()) { CHECK(result->message == "Reply to Alice"); }
  }

  SECTION("messages from other contacts are filtered out")
  {
    auto msg =
      make_message("Hello from Bob", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    CHECK_FALSE(result.has_value());
  }

  SECTION("messages without contact are filtered out")
  {
    auto msg =
      make_message("Some message", core::events::display_message::source::incoming_message, std::nullopt, TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ msg });

    auto result = filtered_queue->try_pop();
    CHECK_FALSE(result.has_value());
  }
}

TEST_CASE("display_filter chat context management", "[display_filter]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();

  auto filtered_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);
  const core::display_filter filter(core::display_filter::out_queues_t{ .filtered = filtered_queue });

  SECTION("enter_chat_mode filters to one contact")
  {
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });

    // Alice's message passes
    auto alice_msg =
      make_message("From Alice", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ alice_msg });

    auto result1 = filtered_queue->try_pop();
    REQUIRE(result1.has_value());
    if (result1.has_value()) { CHECK(result1->message == "From Alice"); }

    // Bob's message filtered
    auto bob_msg =
      make_message("From Bob", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ bob_msg });

    auto result2 = filtered_queue->try_pop();
    CHECK_FALSE(result2.has_value());
  }

  SECTION("exit_chat_mode shows all messages")
  {
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });
    filter.handle(core::events::exit_chat_mode{});

    // Both messages pass after exit
    auto alice_msg =
      make_message("From Alice", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ alice_msg });

    auto bob_msg =
      make_message("From Bob", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ bob_msg });

    auto result1 = filtered_queue->try_pop();
    REQUIRE(result1.has_value());
    if (result1.has_value()) { CHECK(result1->message == "From Alice"); }

    auto result2 = filtered_queue->try_pop();
    REQUIRE(result2.has_value());
    if (result2.has_value()) { CHECK(result2->message == "From Bob"); }
  }

  SECTION("switching chat context changes active contact")
  {
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:bob" });

    // Now Bob's messages pass, Alice's filtered
    auto alice_msg =
      make_message("From Alice", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ alice_msg });

    auto bob_msg =
      make_message("From Bob", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ bob_msg });

    auto result1 = filtered_queue->try_pop();
    REQUIRE(result1.has_value());
    if (result1.has_value()) { CHECK(result1->message == "From Bob"); }

    auto result2 = filtered_queue->try_pop();
    CHECK_FALSE(result2.has_value());
  }
}

TEST_CASE("display_filter switching between contacts", "[display_filter]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();

  auto filtered_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);
  const core::display_filter filter(core::display_filter::out_queues_t{ .filtered = filtered_queue });

  SECTION("switching contacts filters correctly")
  {
    // Start chatting with Alice
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice" });

    auto alice_msg =
      make_message("From Alice", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_1);
    filter.handle(core::events::display_filter_input_t{ alice_msg });

    auto bob_msg =
      make_message("From Bob", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_2);
    filter.handle(core::events::display_filter_input_t{ bob_msg });

    // Alice's message should pass
    auto result1 = filtered_queue->try_pop();
    REQUIRE(result1.has_value());
    if (result1.has_value()) { CHECK(result1->message == "From Alice"); }

    // Bob's message should be filtered
    auto result2 = filtered_queue->try_pop();
    CHECK_FALSE(result2.has_value());

    // Switch to Bob
    filter.handle(core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:bob" });

    auto alice_msg2 = make_message(
      "From Alice again", core::events::display_message::source::incoming_message, "RDX:alice", TIMESTAMP_3);
    filter.handle(core::events::display_filter_input_t{ alice_msg2 });

    auto bob_msg2 =
      make_message("From Bob again", core::events::display_message::source::incoming_message, "RDX:bob", TIMESTAMP_4);
    filter.handle(core::events::display_filter_input_t{ bob_msg2 });

    // Alice's message should be filtered now
    auto result3 = filtered_queue->try_pop();
    REQUIRE(result3.has_value());
    if (result3.has_value()) { CHECK(result3->message == "From Bob again"); }

    // Bob's message should pass
    auto result4 = filtered_queue->try_pop();
    CHECK_FALSE(result4.has_value());
  }
}
