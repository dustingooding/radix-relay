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
    auto output_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);

    WHEN("constructing a TUI processor with output queue")
    {
      THEN("processor can be constructed with all required parameters")
      {
        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> tui_processor(
          "RDX:test123", "hybrid", bridge, command_queue, output_queue);

        REQUIRE(tui_processor.get_mode() == "hybrid");
      }
    }

    WHEN("constructing with different modes")
    {
      THEN("processor stores the mode correctly")
      {
        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> internet_processor(
          "RDX:test456", "internet", bridge, command_queue, output_queue);

        const radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge> mesh_processor(
          "RDX:test789", "mesh", bridge, command_queue, output_queue);

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
    auto output_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);

    auto tui_processor = std::make_unique<radix_relay::tui::processor<radix_relay_test::test_double_signal_bridge>>(
      "RDX:lifecycle", "hybrid", bridge, command_queue, output_queue);

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
