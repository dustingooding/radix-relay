#pragma once

#include <async/async_queue.hpp>
#include <core/events.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

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
  explicit connection_monitor(const std::shared_ptr<async::async_queue<events::display_message>> &display_out_queue)
    : display_out_queue_(display_out_queue)
  {}

  auto handle(const events::transport::connected &event) -> void;
  auto handle(const events::transport::connect_failed &event) -> void;
  auto handle(const events::transport::disconnected &event) -> void;
  auto handle(const events::transport::send_failed &event) -> void;
  auto handle(const events::connection_monitor::query_status &event) -> void;

  [[nodiscard]] auto get_status() const -> connection_status;

private:
  std::shared_ptr<async::async_queue<events::display_message>> display_out_queue_;
  std::unordered_map<events::transport_type, transport_state> states_;
};

}// namespace radix_relay::core
