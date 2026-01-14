#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/events.hpp>
#include <memory>
#include <tui/processor.hpp>

#include "test_doubles/test_double_signal_bridge.hpp"

struct tui_processor_fixture
{
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<radix_relay_test::test_double_signal_bridge> bridge;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::raw_command>> command_queue;
  std::shared_ptr<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>> ui_event_queue;

  tui_processor_fixture()
    : io_context(std::make_shared<boost::asio::io_context>()),
      bridge(std::make_shared<radix_relay_test::test_double_signal_bridge>()),
      command_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context)),
      ui_event_queue(
        std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context))
  {}
};

TEST_CASE("TUI processor can be constructed with all required parameters", "[tui][processor][constructor]")
{
  const tui_processor_fixture fixture;

  const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test123", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  CHECK(tui_processor.get_mode() == "hybrid");
}

TEST_CASE("TUI processor stores mode correctly for internet", "[tui][processor][constructor]")
{
  const tui_processor_fixture fixture;

  const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> internet_processor(
    "RDX:test456", "internet", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  CHECK(internet_processor.get_mode() == "internet");
}

TEST_CASE("TUI processor stores mode correctly for mesh", "[tui][processor][constructor]")
{
  const tui_processor_fixture fixture;

  const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> mesh_processor(
    "RDX:test789", "mesh", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  CHECK(mesh_processor.get_mode() == "mesh");
}

TEST_CASE("TUI processor stop can be called without running", "[tui][processor][lifecycle]")
{
  const tui_processor_fixture fixture;

  auto tui_processor = std::make_unique<radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge>>(
    "RDX:lifecycle", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  REQUIRE_NOTHROW(tui_processor->stop());
}

TEST_CASE("TUI processor destructor properly cleans up", "[tui][processor][lifecycle]")
{
  const tui_processor_fixture fixture;

  auto tui_processor = std::make_unique<radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge>>(
    "RDX:lifecycle", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  REQUIRE_NOTHROW(tui_processor.reset());
}

TEST_CASE("TUI processor get_chat_context returns empty when not in chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  CHECK_FALSE(tui_processor.get_chat_context().has_value());
}

TEST_CASE("TUI processor get_prompt returns default prompt when not in chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  const auto &prompt = tui_processor.get_prompt();
  CHECK(prompt.find("[⇌]") != std::string::npos);
  CHECK(prompt.find('@') == std::string::npos);
}

TEST_CASE("TUI processor get_chat_context returns contact name after entering chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");

  const auto context = tui_processor.get_chat_context();
  REQUIRE(context.has_value());
  if (context.has_value()) { CHECK(context.value() == "alice"); }
}

TEST_CASE("TUI processor get_prompt includes chat indicator when in chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");

  const auto &prompt = tui_processor.get_prompt();
  CHECK(prompt.find("[⇌ @alice]") != std::string::npos);
}

TEST_CASE("TUI processor get_chat_context returns new contact after switching", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");
  tui_processor.update_chat_context("bob");

  const auto context = tui_processor.get_chat_context();
  REQUIRE(context.has_value());
  if (context.has_value()) { CHECK(context.value() == "bob"); }
}

TEST_CASE("TUI processor get_prompt shows new contact after switching", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");
  tui_processor.update_chat_context("bob");

  const auto &prompt = tui_processor.get_prompt();
  CHECK(prompt.find("[⇌ @bob]") != std::string::npos);
  CHECK(prompt.find("alice") == std::string::npos);
}

TEST_CASE("TUI processor get_chat_context returns empty after exiting chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");
  tui_processor.clear_chat_context();

  CHECK_FALSE(tui_processor.get_chat_context().has_value());
}

TEST_CASE("TUI processor get_prompt returns to default after exiting chat mode", "[tui][processor][chat_mode]")
{
  const tui_processor_fixture fixture;

  radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
    "RDX:test", "hybrid", fixture.bridge, fixture.command_queue, fixture.ui_event_queue);

  tui_processor.update_chat_context("alice");
  tui_processor.clear_chat_context();

  const auto &prompt = tui_processor.get_prompt();
  CHECK(prompt.find("[⇌]") != std::string::npos);
  CHECK(prompt.find('@') == std::string::npos);
}
