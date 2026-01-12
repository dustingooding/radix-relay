#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/events.hpp>
#include <memory>
#include <tui/processor.hpp>

#include "test_doubles/test_double_signal_bridge.hpp"

SCENARIO("TUI processor can be constructed with Replxx", "[tui][processor][constructor]")
{
  GIVEN("An io_context, bridge, command queue, and output queue")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
    auto command_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
    auto ui_event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);

    WHEN("constructing a TUI processor with ui_event_queue")
    {
      THEN("processor can be constructed with all required parameters")
      {
        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
          "RDX:test123", "hybrid", bridge, command_queue, ui_event_queue);

        REQUIRE(tui_processor.get_mode() == "hybrid");
      }
    }

    WHEN("constructing with different modes")
    {
      THEN("processor stores the mode correctly")
      {
        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> internet_processor(
          "RDX:test456", "internet", bridge, command_queue, ui_event_queue);

        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> mesh_processor(
          "RDX:test789", "mesh", bridge, command_queue, ui_event_queue);

        REQUIRE(internet_processor.get_mode() == "internet");
        REQUIRE(mesh_processor.get_mode() == "mesh");
      }
    }
  }
}

SCENARIO("TUI processor manages lifecycle correctly", "[tui][processor][lifecycle]")
{
  GIVEN("A constructed TUI processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
    auto command_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
    auto ui_event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);

    auto tui_processor = std::make_unique<radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge>>(
      "RDX:lifecycle", "hybrid", bridge, command_queue, ui_event_queue);

    WHEN("stopping the processor")
    {
      THEN("stop can be called without running") { REQUIRE_NOTHROW(tui_processor->stop()); }
    }

    WHEN("destroying the processor")
    {
      THEN("destructor properly cleans up") { REQUIRE_NOTHROW(tui_processor.reset()); }
    }
  }
}

SCENARIO("TUI processor tracks chat context for prompt display", "[tui][processor][chat_mode]")
{
  GIVEN("A TUI processor")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
    auto command_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
    auto ui_event_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::ui_event_t>>(io_context);

    radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
      "RDX:test", "hybrid", bridge, command_queue, ui_event_queue);

    WHEN("not in chat mode")
    {
      THEN("get_chat_context returns empty optional") { REQUIRE_FALSE(tui_processor.get_chat_context().has_value()); }

      THEN("get_prompt returns default prompt")
      {
        const auto &prompt = tui_processor.get_prompt();
        REQUIRE(prompt.find("[⇌]") != std::string::npos);
        REQUIRE(prompt.find('@') == std::string::npos);
      }
    }

    WHEN("entering chat mode with a contact")
    {
      tui_processor.update_chat_context("alice");

      THEN("get_chat_context returns the contact name")
      {
        const auto context = tui_processor.get_chat_context();
        REQUIRE(context.has_value());
        if (context.has_value()) { CHECK(context.value() == "alice"); }
      }

      THEN("get_prompt includes chat indicator")
      {
        const auto &prompt = tui_processor.get_prompt();
        REQUIRE(prompt.find("[⇌ @alice]") != std::string::npos);
      }
    }

    WHEN("switching between contacts")
    {
      tui_processor.update_chat_context("alice");
      tui_processor.update_chat_context("bob");

      THEN("get_chat_context returns the new contact")
      {
        const auto context = tui_processor.get_chat_context();
        REQUIRE(context.has_value());
        if (context.has_value()) { CHECK(context.value() == "bob"); }
      }

      THEN("get_prompt shows new contact")
      {
        const auto &prompt = tui_processor.get_prompt();
        REQUIRE(prompt.find("[⇌ @bob]") != std::string::npos);
        REQUIRE(prompt.find("alice") == std::string::npos);
      }
    }

    WHEN("exiting chat mode")
    {
      tui_processor.update_chat_context("alice");
      tui_processor.clear_chat_context();

      THEN("get_chat_context returns empty optional") { REQUIRE_FALSE(tui_processor.get_chat_context().has_value()); }

      THEN("get_prompt returns to default")
      {
        const auto &prompt = tui_processor.get_prompt();
        REQUIRE(prompt.find("[⇌]") != std::string::npos);
        REQUIRE(prompt.find('@') == std::string::npos);
      }
    }
  }
}
