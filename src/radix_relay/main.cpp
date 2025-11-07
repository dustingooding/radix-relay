#include <radix_relay/signal/signal_bridge.hpp>
#include <radix_relay/tui/processor.hpp>
#include <spdlog/spdlog.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <cstdlib>
#include <radix_relay/async/async_queue.hpp>
#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/core/command_handler.hpp>
#include <radix_relay/core/command_processor.hpp>
#include <radix_relay/core/event_handler.hpp>
#include <radix_relay/core/events.hpp>
#include <radix_relay/signal/node_identity.hpp>
#include <radix_relay/tui/printer.hpp>
#include <string>
#include <thread>

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  using bridge_t = radix_relay::signal::bridge;

  auto args = radix_relay::cli_utils::parse_cli_args(argc, argv);

  if (not radix_relay::cli_utils::validate_cli_args(args)) { return 1; }

  radix_relay::cli_utils::configure_logging(args);

  auto bridge = std::make_shared<bridge_t>(args.identity_path);
  auto node_fingerprint = bridge->get_node_fingerprint();

  auto io_context = std::make_shared<boost::asio::io_context>();
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);

  auto cli_command_handler = std::make_shared<radix_relay::core::command_handler<bridge_t>>(bridge, display_queue);

  if (radix_relay::cli_utils::execute_cli_command(args, cli_command_handler)) {
    while (auto msg = display_queue->try_pop()) { fmt::print("{}", msg->message); }
    return 0;
  }

  auto command_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::raw_command>>(io_context);

  using cmd_handler_t = radix_relay::core::command_handler<bridge_t>;
  using evt_handler_t = radix_relay::core::event_handler<cmd_handler_t>;
  using cmd_processor_t = radix_relay::core::command_processor<evt_handler_t>;

  auto cmd_handler = std::make_shared<cmd_handler_t>(bridge, display_queue);
  auto evt_handler = std::make_shared<evt_handler_t>(cmd_handler);
  auto cmd_processor = std::make_shared<cmd_processor_t>(io_context, command_queue, evt_handler);

  boost::asio::co_spawn(*io_context, cmd_processor->run(), boost::asio::detached);

  std::thread io_thread([io_context]() { io_context->run(); });

  radix_relay::tui::processor<bridge_t> tui_processor(
    node_fingerprint, args.mode, bridge, command_queue, display_queue);
  tui_processor.run();

  io_context->stop();
  if (io_thread.joinable()) { io_thread.join(); }

  return 0;
}
