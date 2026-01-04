#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace radix_relay::core {

struct transport_state
{
  bool connected;
  std::string url;
  std::string error;
  std::uint64_t timestamp;
};

struct connection_status
{
  std::optional<transport_state> internet;
  std::optional<transport_state> bluetooth;
};

class connection_monitor
{
public:
  // Type traits for standard_processor
  using in_queue_t = async::async_queue<events::connection_monitor::in_t>;

  struct out_queues_t
  {
    std::shared_ptr<async::async_queue<events::display_filter_input_t>> display;
  };

  explicit connection_monitor(const out_queues_t &queues) : display_out_queue_(queues.display) {}

  // Variant handler for standard_processor
  auto handle(const events::connection_monitor::in_t &event) -> void
  {
    std::visit([this](const auto &evt) { this->handle(evt); }, event);
  }

  auto handle(const events::transport::connected &event) -> void;
  auto handle(const events::transport::connect_failed &event) -> void;
  auto handle(const events::transport::disconnected &event) -> void;
  auto handle(const events::transport::send_failed &event) -> void;
  auto handle(const events::connection_monitor::query_status &event) -> void;

  [[nodiscard]] auto get_status() const -> connection_status;

private:
  std::shared_ptr<async::async_queue<events::display_filter_input_t>> display_out_queue_;
  std::unordered_map<events::transport_type, transport_state> states_;
};

}// namespace radix_relay::core
