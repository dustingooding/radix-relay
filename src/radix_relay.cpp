#include <async/async_queue.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <cli_utils/app_init.hpp>
#include <cli_utils/cli_parser.hpp>
#include <core/command_handler.hpp>
#include <core/command_processor.hpp>
#include <core/event_handler.hpp>
#include <core/events.hpp>
#include <core/transport_event_display_handler.hpp>
#include <core/transport_event_processor.hpp>
#include <cstdlib>
#include <nostr/request_tracker.hpp>
#include <nostr/session_orchestrator.hpp>
#include <nostr/transport.hpp>
#include <signal/node_identity.hpp>
#include <signal/signal_bridge.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <transport/websocket_stream.hpp>
#include <tui/printer.hpp>
#include <tui/processor.hpp>

namespace radix_relay {

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  using bridge_t = signal::bridge;

  auto args = cli_utils::parse_cli_args(argc, argv);

  if (not cli_utils::validate_cli_args(args)) { return 1; }

  cli_utils::configure_logging(args);

  auto io_context = std::make_shared<boost::asio::io_context>();

  {
    auto bridge = std::make_shared<bridge_t>(args.identity_path);
    auto node_fingerprint = bridge->get_node_fingerprint();

    auto display_queue = std::make_shared<async::async_queue<core::events::display_message>>(io_context);
    auto transport_queue = std::make_shared<async::async_queue<core::events::transport::in_t>>(io_context);

    auto cli_command_handler =
      std::make_shared<core::command_handler<bridge_t>>(bridge, display_queue, transport_queue);

    if (cli_utils::execute_cli_command(args, cli_command_handler)) {
      while (auto msg = display_queue->try_pop()) { fmt::print("{}", msg->message); }
      return 0;
    }

    auto command_queue = std::make_shared<async::async_queue<core::events::raw_command>>(io_context);

    auto session_in_queue = std::make_shared<async::async_queue<core::events::session_orchestrator::in_t>>(io_context);
    auto main_event_queue = std::make_shared<async::async_queue<core::events::transport_event_variant_t>>(io_context);

    auto request_tracker = std::make_shared<nostr::request_tracker>(io_context.get());
    auto websocket = std::make_shared<transport::websocket_stream>(*io_context);

    auto orchestrator = std::make_shared<nostr::session_orchestrator<bridge_t, nostr::request_tracker>>(
      bridge, request_tracker, io_context, session_in_queue, transport_queue, main_event_queue);

    auto transport = std::make_shared<nostr::transport<transport::websocket_stream>>(
      websocket, io_context, transport_queue, session_in_queue);

    using cmd_handler_t = core::command_handler<bridge_t>;
    using evt_handler_t = core::event_handler<cmd_handler_t>;
    using cmd_processor_t = core::command_processor<evt_handler_t>;
    using transport_evt_handler_t = core::transport_event_display_handler;
    using transport_evt_processor_t = core::transport_event_processor<transport_evt_handler_t>;

    auto cmd_handler = std::make_shared<cmd_handler_t>(bridge, display_queue, transport_queue);
    auto evt_handler = std::make_shared<evt_handler_t>(cmd_handler);
    auto cmd_processor = std::make_shared<cmd_processor_t>(io_context, command_queue, evt_handler);

    auto transport_evt_handler = std::make_shared<transport_evt_handler_t>(display_queue);
    auto transport_evt_processor =
      std::make_shared<transport_evt_processor_t>(io_context, main_event_queue, transport_evt_handler);

    auto work_guard = boost::asio::make_work_guard(*io_context);

    auto spawn_orchestrator_loop =
      [](const std::shared_ptr<boost::asio::io_context> &ctx,
        std::shared_ptr<nostr::session_orchestrator<bridge_t, nostr::request_tracker>> orch) {
        boost::asio::co_spawn(
          *ctx,
          // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
          [orchestrator = std::move(orch)]() -> boost::asio::awaitable<void> { co_await orchestrator->run(); },
          boost::asio::detached);
      };

    auto spawn_transport_loop = [](const std::shared_ptr<boost::asio::io_context> &ctx,
                                  std::shared_ptr<nostr::transport<transport::websocket_stream>> trans) {
      boost::asio::co_spawn(
        *ctx,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [transport = std::move(trans)]() -> boost::asio::awaitable<void> { co_await transport->run(); },
        boost::asio::detached);
    };

    auto spawn_command_processor_loop = [](const std::shared_ptr<boost::asio::io_context> &ctx,
                                          std::shared_ptr<cmd_processor_t> proc) {
      boost::asio::co_spawn(
        *ctx,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [cmd_processor = std::move(proc)]() -> boost::asio::awaitable<void> { co_await cmd_processor->run(); },
        boost::asio::detached);
    };

    auto spawn_transport_event_processor_loop = [](const std::shared_ptr<boost::asio::io_context> &ctx,
                                                  std::shared_ptr<transport_evt_processor_t> proc) {
      boost::asio::co_spawn(
        *ctx,
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        [transport_evt_processor = std::move(proc)]() -> boost::asio::awaitable<void> {
          co_await transport_evt_processor->run();
        },
        boost::asio::detached);
    };

    spawn_orchestrator_loop(io_context, orchestrator);
    spawn_transport_loop(io_context, transport);
    spawn_command_processor_loop(io_context, cmd_processor);
    spawn_transport_event_processor_loop(io_context, transport_evt_processor);

    std::thread io_thread([&io_context]() {
      spdlog::debug("io_context thread started");
      io_context->run();
      spdlog::debug("io_context thread stopped");
    });

    tui::processor<bridge_t> tui_processor(node_fingerprint, args.mode, bridge, command_queue, display_queue);
    tui_processor.run();

    // TODO: remove when cancellation_signal implemented
    spdlog::debug("Sending disconnect command...");
    transport_queue->push(core::events::transport::disconnect{});
    const auto disconnect_wait{ 100 };
    std::this_thread::sleep_for(std::chrono::milliseconds(disconnect_wait));

    // TODO: remove when cancellation_signal implemented
    spdlog::debug("Canceling all queues...");
    display_queue->cancel();
    transport_queue->cancel();
    command_queue->cancel();
    session_in_queue->cancel();
    main_event_queue->cancel();

    spdlog::debug("Resetting work guard...");
    work_guard.reset();

    spdlog::debug("Waiting for io_context to drain...");
    if (io_thread.joinable()) { io_thread.join(); }
  }

  return 0;
}

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int { return radix_relay::main(argc, argv); }
