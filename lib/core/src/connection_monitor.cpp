#include <chrono>
#include <core/connection_monitor.hpp>
#include <fmt/format.h>

namespace radix_relay::core {

auto connection_monitor::handle(const events::transport::connected &event) -> void
{
  auto timestamp = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  states_[event.type] = transport_state{ .connected = true, .url = event.url, .error = "", .timestamp = timestamp };
}

auto connection_monitor::handle(const events::transport::connect_failed &event) -> void
{
  auto timestamp = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  states_[event.type] =
    transport_state{ .connected = false, .url = event.url, .error = event.error_message, .timestamp = timestamp };
}

auto connection_monitor::handle(const events::transport::disconnected &event) -> void
{
  auto timestamp = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  if (states_.contains(event.type)) {
    states_[event.type].connected = false;
    states_[event.type].url = "";
    states_[event.type].error = "";
    states_[event.type].timestamp = timestamp;
  }
}

auto connection_monitor::handle(const events::transport::send_failed &event) -> void
{
  auto timestamp = static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

  if (states_.contains(event.type)) {
    states_[event.type].error = event.error_message;
    states_[event.type].timestamp = timestamp;
  }
}

auto connection_monitor::handle(const events::connection_monitor::query_status & /*event*/) -> void
{
  auto status = get_status();

  std::string internet_status = "Not connected";
  if (status.internet.has_value()) {
    if (status.internet->connected) {
      internet_status = fmt::format("Connected ({})", status.internet->url);
    } else if (not status.internet->error.empty()) {
      internet_status = fmt::format("Failed ({})", status.internet->error);
    }
  }

  std::string bluetooth_status = "Not connected";
  if (status.bluetooth.has_value()) {
    if (status.bluetooth->connected) {
      bluetooth_status = fmt::format("Connected ({})", status.bluetooth->url);
    } else if (not status.bluetooth->error.empty()) {
      bluetooth_status = fmt::format("Failed ({})", status.bluetooth->error);
    }
  }

  auto message = fmt::format(
    "Network Status:\n  Internet: {}\n  BLE Mesh: {}\n  Active Sessions: 0\n", internet_status, bluetooth_status);
  display_out_queue_->push(events::display_message{ message });
}

auto connection_monitor::get_status() const -> connection_status
{
  connection_status status;

  if (states_.contains(events::transport_type::internet)) {
    status.internet = states_.at(events::transport_type::internet);
  }

  if (states_.contains(events::transport_type::bluetooth)) {
    status.bluetooth = states_.at(events::transport_type::bluetooth);
  }

  return status;
}

}// namespace radix_relay::core
