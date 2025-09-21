#include <radix_relay/cli.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <spdlog/spdlog.h>

#include "internal_use_only/config.hpp"
#include <cstdlib>
#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/node_identity.hpp>
#include <string>

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  auto args = radix_relay::parse_cli_args(argc, argv);

  if (not radix_relay::validate_cli_args(args)) { return 1; }

  radix_relay::configure_logging(args);

  auto bridge = radix_relay::new_signal_bridge(args.identity_path.c_str());
  auto node_fingerprint = radix_relay::get_node_fingerprint(*bridge);
  const radix_relay::StandardEventHandler::command_handler_t command_handler{ std::move(bridge) };

  if (radix_relay::execute_cli_command(args, command_handler)) { return 0; }
  radix_relay::print_app_banner(
    { .node_fingerprint = node_fingerprint, .mode = args.mode, .identity_path = args.identity_path });
  radix_relay::print_available_commands();

  const radix_relay::StandardEventHandler event_handler{ command_handler };
  radix_relay::InteractiveCli cli(node_fingerprint, args.mode, event_handler);
  cli.run();

  return 0;
}
