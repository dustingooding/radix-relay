#include <catch2/catch_test_macros.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>

using namespace radix_relay::core;

TEST_CASE("connection_monitor initially has empty transport states", "[connection_monitor][internet]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  const connection_monitor monitor(queues);

  auto status = monitor.get_status();

  CHECK_FALSE(status.internet.has_value());
  CHECK_FALSE(status.bluetooth.has_value());
}

TEST_CASE("connection_monitor tracks internet transport connection success", "[connection_monitor][internet]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected event{ .url = "wss://relay.damus.io", .type = events::transport_type::internet };

  monitor.handle(event);
  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &state = *status.internet;
    CHECK(state.connected);
    CHECK(state.url == "wss://relay.damus.io");
    CHECK(state.error.empty());
    CHECK(state.timestamp > 0);
  }
}

TEST_CASE("connection_monitor internet connection does not affect bluetooth state", "[connection_monitor][internet]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected event{ .url = "wss://relay.damus.io", .type = events::transport_type::internet };

  monitor.handle(event);
  auto status = monitor.get_status();

  CHECK_FALSE(status.bluetooth.has_value());
}

TEST_CASE("connection_monitor tracks internet transport connection failure", "[connection_monitor][internet]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connect_failed event{ .url = "wss://bad-relay.example.com",
    .error_message = "Connection timeout",
    .type = events::transport_type::internet };

  monitor.handle(event);
  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &state = *status.internet;
    CHECK_FALSE(state.connected);
    CHECK(state.url == "wss://bad-relay.example.com");
    CHECK(state.error == "Connection timeout");
    CHECK(state.timestamp > 0);
  }
}

TEST_CASE("connection_monitor tracks internet transport disconnection", "[connection_monitor][internet]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected connect_event{ .url = "wss://relay.damus.io",
    .type = events::transport_type::internet };
  monitor.handle(connect_event);

  const events::transport::disconnected disconnect_event{ .type = events::transport_type::internet };
  monitor.handle(disconnect_event);

  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &state = *status.internet;
    CHECK_FALSE(state.connected);
    CHECK(state.error.empty());
  }
}

TEST_CASE("connection_monitor tracks bluetooth transport connection success", "[connection_monitor][bluetooth]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected event{ .url = "ble://device-123", .type = events::transport_type::bluetooth };

  monitor.handle(event);
  auto status = monitor.get_status();

  REQUIRE(status.bluetooth.has_value());
  if (status.bluetooth.has_value()) {
    const auto &state = *status.bluetooth;
    CHECK(state.connected);
    CHECK(state.url == "ble://device-123");
    CHECK(state.error.empty());
    CHECK(state.timestamp > 0);
  }
}

TEST_CASE("connection_monitor bluetooth connection does not affect internet state", "[connection_monitor][bluetooth]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected event{ .url = "ble://device-123", .type = events::transport_type::bluetooth };

  monitor.handle(event);
  auto status = monitor.get_status();

  CHECK_FALSE(status.internet.has_value());
}

TEST_CASE("connection_monitor handles both internet and bluetooth connections simultaneously",
  "[connection_monitor][hybrid]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected internet_event{ .url = "wss://relay.damus.io",
    .type = events::transport_type::internet };
  const events::transport::connected bluetooth_event{ .url = "ble://device-123",
    .type = events::transport_type::bluetooth };

  monitor.handle(internet_event);
  monitor.handle(bluetooth_event);

  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &inet_state = *status.internet;
    CHECK(inet_state.connected);
    CHECK(inet_state.url == "wss://relay.damus.io");
  }

  REQUIRE(status.bluetooth.has_value());
  if (status.bluetooth.has_value()) {
    const auto &bt_state = *status.bluetooth;
    CHECK(bt_state.connected);
    CHECK(bt_state.url == "ble://device-123");
  }
}

TEST_CASE("connection_monitor clears error state on successful connection", "[connection_monitor][error_recovery]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connect_failed fail_event{ .url = "wss://bad-relay.example.com",
    .error_message = "DNS resolution failed",
    .type = events::transport_type::internet };
  monitor.handle(fail_event);

  const events::transport::connected success_event{ .url = "wss://good-relay.example.com",
    .type = events::transport_type::internet };
  monitor.handle(success_event);

  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &state = *status.internet;
    CHECK(state.connected);
    CHECK(state.error.empty());
    CHECK(state.url == "wss://good-relay.example.com");
  }
}

TEST_CASE("connection_monitor handles send_failed events while remaining connected", "[connection_monitor][send]")
{
  const connection_monitor::out_queues_t queues{ .display = nullptr };
  connection_monitor monitor(queues);

  const events::transport::connected connect_event{ .url = "wss://relay.damus.io",
    .type = events::transport_type::internet };
  monitor.handle(connect_event);

  const events::transport::send_failed fail_event{
    .message_id = "msg-456", .error_message = "Send timeout", .type = events::transport_type::internet
  };
  monitor.handle(fail_event);

  auto status = monitor.get_status();

  REQUIRE(status.internet.has_value());
  if (status.internet.has_value()) {
    const auto &state = *status.internet;
    CHECK(state.connected);
    CHECK(state.error == "Send timeout");
  }
}

TEST_CASE("connection_monitor emits display message for query_status with connected internet",
  "[connection_monitor][query]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>>(io_context);
  const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
  radix_relay::core::connection_monitor monitor(queues);

  const radix_relay::core::events::transport::connected connected_event{ .url = "wss://relay.example.com",
    .type = radix_relay::core::events::transport_type::internet };
  monitor.handle(connected_event);

  const radix_relay::core::events::connection_monitor::query_status query{};
  monitor.handle(query);

  auto msg = display_queue->try_pop();
  REQUIRE(msg.has_value());
  if (msg.has_value()) {
    std::visit(
      [](const auto &evt) {
        if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
          CHECK(evt.message.find("Network Status") != std::string::npos);
          CHECK(evt.message.find("Internet: Connected (wss://relay.example.com)") != std::string::npos);
          CHECK(evt.message.find("BLE Mesh:") != std::string::npos);
        }
      },
      *msg);
  }
}

TEST_CASE("connection_monitor emits display message for query_status with disconnected internet",
  "[connection_monitor][query]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>>(io_context);
  const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
  radix_relay::core::connection_monitor monitor(queues);

  const radix_relay::core::events::transport::disconnected disconnected_event{
    .type = radix_relay::core::events::transport_type::internet
  };
  monitor.handle(disconnected_event);

  const radix_relay::core::events::connection_monitor::query_status query{};
  monitor.handle(query);

  auto msg = display_queue->try_pop();
  REQUIRE(msg.has_value());
  if (msg.has_value()) {
    std::visit(
      [](const auto &evt) {
        if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
          CHECK(evt.message.find("Internet: Not connected") != std::string::npos);
        }
      },
      *msg);
  }
}

TEST_CASE("connection_monitor emits display message for query_status with failed connection",
  "[connection_monitor][query]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  auto display_queue =
    std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_filter_input_t>>(io_context);
  const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
  radix_relay::core::connection_monitor monitor(queues);

  const radix_relay::core::events::transport::connect_failed failed_event{ .url = "wss://relay.fail.com",
    .error_message = "Connection timeout",
    .type = radix_relay::core::events::transport_type::internet };
  monitor.handle(failed_event);

  const radix_relay::core::events::connection_monitor::query_status query{};
  monitor.handle(query);

  auto msg = display_queue->try_pop();
  REQUIRE(msg.has_value());
  if (msg.has_value()) {
    std::visit(
      [](const auto &evt) {
        if constexpr (std::same_as<std::decay_t<decltype(evt)>, radix_relay::core::events::display_message>) {
          CHECK(evt.message.find("Internet: Failed (Connection timeout)") != std::string::npos);
        }
      },
      *msg);
  }
}
