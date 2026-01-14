#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/events.hpp>
#include <gui/processor.hpp>
#include <memory>
#include <slint-testing.h>

#include "test_doubles/test_double_signal_bridge.hpp"

namespace {
struct slint_test_init
{
  slint_test_init() noexcept { slint::testing::init(); }
};
// NOLINTNEXTLINE(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
[[maybe_unused]] const slint_test_init slint_init_once;
}// namespace

TEST_CASE("processor polls ui_event_queue and updates message model", "[gui][processor]")
{

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  ui_event_queue->push(radix_relay::core::events::display_message{ .message = "Test message 1",
    .contact_rdx = std::nullopt,
    .timestamp = 0,
    .source_type = radix_relay::core::events::display_message::source::system });
  ui_event_queue->push(radix_relay::core::events::display_message{ .message = "Test message 2",
    .contact_rdx = std::nullopt,
    .timestamp = 0,
    .source_type = radix_relay::core::events::display_message::source::system });

  processor.poll_ui_events();

  CHECK(message_model->row_count() == 2);
}

TEST_CASE("processor handles /quit command", "[gui][processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  window->invoke_send_command("/quit");

  CHECK(processor.is_running() == false);
}

TEST_CASE("processor pushes commands to command queue", "[gui][processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  const radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  window->invoke_send_command("/help");

  auto cmd = command_queue->try_pop();
  REQUIRE(cmd.has_value());
  if (cmd.has_value()) { CHECK(cmd->input == "/help"); }
}

TEST_CASE("processor rate-limits message processing to prevent UI starvation", "[gui][processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  constexpr std::size_t total_messages = 25;
  for (std::size_t i = 0; i < total_messages; ++i) {
    ui_event_queue->push(radix_relay::core::events::display_message{ .message = "Message " + std::to_string(i),
      .contact_rdx = std::nullopt,
      .timestamp = 0,
      .source_type = radix_relay::core::events::display_message::source::system });
  }

  processor.poll_ui_events();

  constexpr std::size_t max_per_poll = 10;
  CHECK(message_model->row_count() == max_per_poll);

  CHECK(ui_event_queue->size() == total_messages - max_per_poll);

  processor.poll_ui_events();
  CHECK(message_model->row_count() == max_per_poll * 2);
}

TEST_CASE("processor tracks chat context for UI display", "[gui][processor][chat_mode]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto ui_event_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);
  auto window = radix_relay::gui::make_window();
  auto message_model = radix_relay::gui::make_message_model();

  radix_relay::gui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue, window, message_model);

  SECTION("not in chat mode by default")
  {
    CHECK_FALSE(processor.get_chat_context().has_value());
    CHECK(window->get_active_chat_contact().empty());
  }

  SECTION("entering chat mode via event")
  {
    ui_event_queue->push(
      radix_relay::core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice", .display_name = "alice" });

    processor.poll_ui_events();

    const auto context = processor.get_chat_context();
    REQUIRE(context.has_value());
    if (context.has_value()) { CHECK(context.value() == "alice"); }
    CHECK(window->get_active_chat_contact() == "alice");
  }

  SECTION("exiting chat mode via event")
  {
    processor.update_chat_context("alice");

    ui_event_queue->push(radix_relay::core::events::exit_chat_mode{});

    processor.poll_ui_events();

    CHECK_FALSE(processor.get_chat_context().has_value());
    CHECK(window->get_active_chat_contact().empty());
  }

  SECTION("switching between contacts via events")
  {
    ui_event_queue->push(
      radix_relay::core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:alice", .display_name = "alice" });
    processor.poll_ui_events();
    auto context1 = processor.get_chat_context();
    if (context1.has_value()) { CHECK(context1.value() == "alice"); }

    ui_event_queue->push(
      radix_relay::core::events::enter_chat_mode{ .rdx_fingerprint = "RDX:bob", .display_name = "bob" });
    processor.poll_ui_events();
    auto context2 = processor.get_chat_context();
    if (context2.has_value()) { CHECK(context2.value() == "bob"); }
    CHECK(window->get_active_chat_contact() == "bob");
  }
}
