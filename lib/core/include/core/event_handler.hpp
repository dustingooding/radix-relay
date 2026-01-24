#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <memory>
#include <variant>

namespace radix_relay::core {

/**
 * @brief Dispatches parsed command events to the command handler.
 *
 * @tparam CommandHandler Callable type that handles typed command events via operator()
 * @tparam CommandParser Parser type with command_variant_t type trait and parse() method
 *
 * Uses Chain of Responsibility (parser) + Visitor (command_handler) pattern:
 * - Parser converts raw string to strongly-typed command variant
 * - std::visit dispatches the variant to command_handler
 */
template<typename CommandHandler, typename CommandParser> struct event_handler
{
  using in_queue_t = async::async_queue<events::raw_command>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::display_filter_input_t>> display;
    std::shared_ptr<async::async_queue<events::transport::in_t>> transport;
    std::shared_ptr<async::async_queue<events::session_orchestrator::in_t>> session;
    std::shared_ptr<async::async_queue<events::connection_monitor::in_t>> connection_monitor;
  };

  /**
   * @brief Constructs an event handler with command handler and parser.
   *
   * @param command_handler Command handler that handles typed commands (shared ownership)
   * @param parser Command parser for converting strings to typed events
   * @param queues Output queues (stored for type compatibility, handler uses its own references)
   */
  explicit event_handler(std::shared_ptr<CommandHandler> command_handler,
    std::shared_ptr<CommandParser> parser,
    const out_queues_t & /*queues*/)
    : command_handler_(std::move(command_handler)), parser_(std::move(parser))
  {}

  /**
   * @brief Parses and handles a raw command string.
   *
   * Parses input to typed command and dispatches via std::visit.
   *
   * @param event Raw command event containing unparsed user input
   */
  auto handle(const events::raw_command &event) const -> void
  {
    auto command = parser_->parse(event.input);
    std::visit(*command_handler_, command);
  }

private:
  std::shared_ptr<CommandHandler> command_handler_;
  std::shared_ptr<CommandParser> parser_;
};

}// namespace radix_relay::core
