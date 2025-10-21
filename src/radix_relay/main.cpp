#include <radix_relay/cli.hpp>
#include <radix_relay/signal_bridge.hpp>
#include <radix_relay/standard_event_handler.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/node_identity.hpp>
#include <string>

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  using bridge_t = radix_relay::signal::bridge;

  auto args = radix_relay::parse_cli_args(argc, argv);

  if (not radix_relay::validate_cli_args(args)) { return 1; }

  radix_relay::configure_logging(args);

  auto bridge = std::make_shared<bridge_t>(args.identity_path);
  auto node_fingerprint = bridge->get_node_fingerprint();

  auto command_handler = std::make_shared<radix_relay::command_handler<bridge_t>>(bridge);

  if (radix_relay::execute_cli_command(args, command_handler)) { return 0; }
  radix_relay::print_app_banner(
    { .node_fingerprint = node_fingerprint, .mode = args.mode, .identity_path = args.identity_path });
  radix_relay::print_available_commands();

  auto event_handler = std::make_shared<radix_relay::standard_event_handler_t<bridge_t>>(command_handler);
  radix_relay::interactive_cli cli(node_fingerprint, args.mode, event_handler);
  cli.run();

  return 0;
}
