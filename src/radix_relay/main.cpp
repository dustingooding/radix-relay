#include <radix_relay/signal/signal_bridge.hpp>
#include <radix_relay/tui/processor.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <radix_relay/cli_utils/app_init.hpp>
#include <radix_relay/cli_utils/cli_parser.hpp>
#include <radix_relay/signal/node_identity.hpp>
#include <string>

// NOLINTNEXTLINE(bugprone-exception-escape)
auto main(int argc, char **argv) -> int
{
  using bridge_t = radix_relay::signal::bridge;

  auto args = radix_relay::cli_utils::parse_cli_args(argc, argv);

  if (not radix_relay::cli_utils::validate_cli_args(args)) { return 1; }

  radix_relay::cli_utils::configure_logging(args);

  auto bridge = std::make_shared<bridge_t>(args.identity_path);
  auto node_fingerprint = bridge->get_node_fingerprint();

  auto command_handler = std::make_shared<radix_relay::core::command_handler<bridge_t>>(bridge);

  if (radix_relay::cli_utils::execute_cli_command(args, command_handler)) { return 0; }

  radix_relay::tui::processor tui_processor(node_fingerprint, args.mode, bridge);
  tui_processor.run();

  return 0;
}
