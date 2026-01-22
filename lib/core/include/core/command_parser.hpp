#pragma once

#include <concepts/signal_bridge.hpp>
#include <core/events.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace radix_relay::core {

/**
 * @brief Chain of Responsibility command parser.
 *
 * Parses raw command strings into strongly-typed command events.
 * Defines its own variant type as a type trait so types and parsing always match.
 * Uses unknown_command as fallback when no command matches.
 * Manages chat mode state for preprocessing plain text as send commands.
 *
 * @tparam Bridge Type satisfying the signal_bridge concept (for contact lookup)
 */
template<concepts::signal_bridge Bridge> struct command_parser
{
  using command_variant_t = std::variant<events::help,
    events::peers,
    events::status,
    events::sessions,
    events::identities,
    events::scan,
    events::version,
    events::mode,
    events::send,
    events::broadcast,
    events::connect,
    events::disconnect,
    events::publish_identity,
    events::unpublish_identity,
    events::trust,
    events::verify,
    events::chat,
    events::leave,
    events::unknown_command>;

  using parse_result_t = std::optional<command_variant_t>;
  using handler_t = std::function<parse_result_t(const std::string &)>;

  explicit command_parser(std::shared_ptr<Bridge> bridge) : bridge_(std::move(bridge)) { build_handler_chain(); }

  /**
   * @brief Parses a raw command string into a typed command event.
   *
   * When in chat mode, plain text (not starting with /) is converted to a send command.
   * The /leave command exits chat mode.
   *
   * @param input Raw command string
   * @return Strongly-typed command variant
   */
  [[nodiscard]] auto parse(const std::string &input) const -> command_variant_t
  {
    auto effective_input = input;

    if (active_chat_rdx_.has_value() and not input.starts_with("/")) {
      effective_input = "/send " + active_chat_rdx_.value() + " " + input;
    }

    for (const auto &handler : handlers_) {
      // cppcheck-suppress useStlAlgorithm
      if (auto result = handler(effective_input)) { return *result; }
    }

    return events::unknown_command{ .input = effective_input };
  }

  /**
   * @brief Enters chat mode with the specified contact.
   *
   * @param rdx_fingerprint The RDX fingerprint of the contact to chat with
   */
  auto enter_chat_mode(const std::string &rdx_fingerprint) const -> void { active_chat_rdx_ = rdx_fingerprint; }

  /**
   * @brief Exits chat mode.
   */
  auto exit_chat_mode() const -> void { active_chat_rdx_.reset(); }

  /**
   * @brief Checks if currently in chat mode.
   *
   * @return true if in chat mode, false otherwise
   */
  [[nodiscard]] auto in_chat_mode() const -> bool { return active_chat_rdx_.has_value(); }

private:
  std::shared_ptr<Bridge> bridge_;
  mutable std::optional<std::string> active_chat_rdx_;
  std::vector<handler_t> handlers_;

  /**
   * @brief Creates a handler for exact command matches (no arguments).
   */
  template<typename Event> static auto exact_match(std::string_view command) -> handler_t
  {
    return [cmd = std::string(command)](const std::string &input) -> parse_result_t {
      if (input == cmd) { return Event{}; }
      return std::nullopt;
    };
  }

  /**
   * @brief Creates a handler for prefix commands with a single argument.
   */
  template<typename Event, typename Extractor>
  static auto prefix_match(std::string_view prefix, Extractor extractor) -> handler_t
  {
    return [p = std::string(prefix), ext = std::move(extractor)](const std::string &input) -> parse_result_t {
      if (input.starts_with(p)) { return ext(input.substr(p.length())); }
      return std::nullopt;
    };
  }

  auto build_handler_chain() -> void
  {
    // Handlers ordered by expected frequency of use (most common first)

    // /send <peer> <message> - most frequent in chat mode
    handlers_.push_back(prefix_match<events::send>("/send ", [](const std::string &args) {
      const auto first_space = args.find(' ');
      if (first_space != std::string::npos and not args.empty()) {
        return events::send{ .peer = args.substr(0, first_space), .message = args.substr(first_space + 1) };
      }
      return events::send{ .peer = "", .message = "" };
    }));

    // /chat <contact> - entering conversations
    handlers_.push_back([this](const std::string &input) -> parse_result_t {
      constexpr auto chat_cmd = "/chat ";
      if (not input.starts_with(chat_cmd)) { return std::nullopt; }

      const auto contact_name = input.substr(std::string_view(chat_cmd).length());
      if (not contact_name.empty()) {
        try {
          const auto contact = bridge_->lookup_contact(contact_name);
          active_chat_rdx_ = contact.rdx_fingerprint;
        } catch (const std::exception & /*e*/) {
          // Contact lookup failed, don't enter chat mode
        }
      }
      return events::chat{ .contact = contact_name };
    });

    // /leave - exiting conversations
    handlers_.push_back([this](const std::string &input) -> parse_result_t {
      if (input == "/leave") {
        active_chat_rdx_.reset();
        return events::leave{};
      }
      return std::nullopt;
    });

    // Common informational commands
    handlers_.push_back(exact_match<events::help>("/help"));
    handlers_.push_back(exact_match<events::status>("/status"));
    handlers_.push_back(exact_match<events::peers>("/peers"));
    handlers_.push_back(exact_match<events::sessions>("/sessions"));

    // Connection management
    handlers_.push_back(prefix_match<events::connect>(
      "/connect ", [](const std::string &args) { return events::connect{ .relay = args }; }));
    handlers_.push_back(exact_match<events::disconnect>("/disconnect"));

    // Contact/identity management
    handlers_.push_back(exact_match<events::identities>("/identities"));
    handlers_.push_back(prefix_match<events::trust>("/trust ", [](const std::string &args) {
      const auto first_space = args.find(' ');
      if (first_space != std::string::npos and not args.empty()) {
        return events::trust{ .peer = args.substr(0, first_space), .alias = args.substr(first_space + 1) };
      }
      return events::trust{ .peer = args, .alias = "" };
    }));
    handlers_.push_back(
      prefix_match<events::verify>("/verify ", [](const std::string &args) { return events::verify{ .peer = args }; }));

    // Less frequent commands
    handlers_.push_back(prefix_match<events::broadcast>(
      "/broadcast ", [](const std::string &args) { return events::broadcast{ .message = args }; }));
    handlers_.push_back(
      prefix_match<events::mode>("/mode ", [](const std::string &args) { return events::mode{ .new_mode = args }; }));
    handlers_.push_back(exact_match<events::scan>("/scan"));
    handlers_.push_back(exact_match<events::version>("/version"));
    handlers_.push_back(exact_match<events::publish_identity>("/publish"));
    handlers_.push_back(exact_match<events::unpublish_identity>("/unpublish"));
  }
};

}// namespace radix_relay::core
