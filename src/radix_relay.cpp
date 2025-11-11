#include <async/async_queue.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <cli_utils/app_init.hpp>
#include <cli_utils/cli_parser.hpp>
#include <core/command_handler.hpp>
#include <core/command_processor.hpp>
#include <core/event_handler.hpp>
#include <core/events.hpp>
#include <core/processor_runner.hpp>
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
  auto cancel_signal = std::make_shared<boost::asio::cancellation_signal>();
  auto cancel_slot = std::make_shared<boost::asio::cancellation_slot>(cancel_signal->slot());

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

    auto request_tracker = std::make_shared<nostr::request_tracker>(io_context);
    auto websocket = std::make_shared<transport::websocket_stream>(io_context);

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

    auto orch_state = core::spawn_processor(io_context, orchestrator, cancel_slot, "session_orchestrator");
    auto transport_state = core::spawn_processor(io_context, transport, cancel_slot, "transport");
    auto cmd_proc_state = core::spawn_processor(io_context, cmd_processor, cancel_slot, "command_processor");
    auto evt_proc_state =
      core::spawn_processor(io_context, transport_evt_processor, cancel_slot, "transport_event_processor");

    std::thread io_thread([&io_context]() {
      spdlog::debug("io_context thread started");
      io_context->run();
      spdlog::debug("io_context thread stopped");
    });

    tui::processor<bridge_t> tui_processor(node_fingerprint, args.mode, bridge, command_queue, display_queue);
    tui_processor.run();

    spdlog::debug("TUI exited, posting cancellation signal to io_context thread...");
    boost::asio::post(*io_context, [cancel_signal]() {
      spdlog::debug("[main] Emitting cancellation signal on io_context thread");
      cancel_signal->emit(boost::asio::cancellation_type::all);
    });

    spdlog::debug("Closing all queues...");
    command_queue->close();
    display_queue->close();
    session_in_queue->close();
    transport_queue->close();
    main_event_queue->close();

    spdlog::debug("Waiting for coroutines to complete...");
    auto start = std::chrono::steady_clock::now();
    constexpr auto timeout = std::chrono::seconds(2);
    while ((orch_state->started != orch_state->done) or (transport_state->started != transport_state->done)
           or (cmd_proc_state->started != cmd_proc_state->done) or (evt_proc_state->started != evt_proc_state->done)) {
      const auto coroutine_complete_wait{ 10 };
      std::this_thread::sleep_for(std::chrono::milliseconds(coroutine_complete_wait));
      if (std::chrono::steady_clock::now() - start > timeout) {
        spdlog::warn("Timeout waiting for coroutines to complete, forcing shutdown");
        spdlog::debug("Coroutine states: orch({}/{}), transport({}/{}), cmd({}/{}), evt({}/{})",
          orch_state->started.load(),
          orch_state->done.load(),
          transport_state->started.load(),
          transport_state->done.load(),
          cmd_proc_state->started.load(),
          cmd_proc_state->done.load(),
          evt_proc_state->started.load(),
          evt_proc_state->done.load());
        break;
      }
    }

    spdlog::debug("Resetting work guard...");
    work_guard.reset();

    spdlog::debug("Stopping io_context...");
    io_context->stop();

    spdlog::debug("Waiting for io_thread to join...");
    if (io_thread.joinable()) {
      io_thread.join();
      spdlog::debug("io_thread joined");
    }

    spdlog::debug("Cleaning up resources...");
  }

  return 0;
}

}// namespace radix_relay

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int { return radix_relay::main(argc, argv); }
