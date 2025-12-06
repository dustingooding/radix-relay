#include <async/async_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <catch2/catch_test_macros.hpp>
#include <core/events.hpp>
#include <main_window.h>
#include <memory>
#include <slint-testing.h>
#include <slint_ui/processor.hpp>

#include "test_doubles/test_double_signal_bridge.hpp"

namespace {
struct slint_test_init
{
  slint_test_init() noexcept { slint::testing::init(); }
};
// NOLINTNEXTLINE(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
[[maybe_unused]] const slint_test_init slint_init_once;
}// namespace

TEST_CASE("processor polls display_queue and updates message model", "[slint_ui][processor]")
{

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
  auto window = MainWindow::create();
  auto message_model = std::make_shared<slint::VectorModel<Message>>();

  radix_relay::slint_ui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, display_queue, window, message_model);

  display_queue->push(radix_relay::core::events::display_message{ .message = "Test message 1" });
  display_queue->push(radix_relay::core::events::display_message{ .message = "Test message 2" });

  processor.poll_display_messages();

  CHECK(message_model->row_count() == 2);
}

TEST_CASE("processor handles /quit command", "[slint_ui][processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
  auto window = MainWindow::create();
  auto message_model = std::make_shared<slint::VectorModel<Message>>();

  const radix_relay::slint_ui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, display_queue, window, message_model);

  window->invoke_send_command("/quit");

  CHECK(processor.is_running() == false);
}

TEST_CASE("processor pushes commands to command queue", "[slint_ui][processor]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto bridge = std::make_shared<radix_relay_test::test_double_signal_bridge>();
  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
  auto window = MainWindow::create();
  auto message_model = std::make_shared<slint::VectorModel<Message>>();

  const radix_relay::slint_ui::processor<radix_relay_test::test_double_signal_bridge> processor(
    "RDX:test123", "hybrid", bridge, command_queue, display_queue, window, message_model);

  window->invoke_send_command("/help");

  auto cmd = command_queue->try_pop();
  REQUIRE(cmd.has_value());
  if (cmd.has_value()) { CHECK(cmd->input == "/help"); }
}
