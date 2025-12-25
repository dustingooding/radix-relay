#include <catch2/catch_test_macros.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>

using namespace radix_relay::core;

SCENARIO("connection_monitor tracks internet transport connection state", "[connection_monitor][internet]")
{
  GIVEN("A connection_monitor instance")
  {
    const connection_monitor::out_queues_t queues{ .display = nullptr };
    connection_monitor monitor(queues);

    WHEN("initially created")
    {
      auto status = monitor.get_status();

      THEN("both transport states are empty")
      {
        CHECK_FALSE(status.internet.has_value());
        CHECK_FALSE(status.bluetooth.has_value());
      }
    }

    WHEN("internet transport connects successfully")
    {
      const events::transport::connected event{ .url = "wss://relay.damus.io",
        .type = events::transport_type::internet };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("internet state shows connected")
      {
        REQUIRE(status.internet.has_value());
        if (status.internet.has_value()) {
          const auto &state = *status.internet;
          CHECK(state.connected);
          CHECK(state.url == "wss://relay.damus.io");
          CHECK(state.error.empty());
          CHECK(state.timestamp > 0);
        }
      }

      THEN("bluetooth state remains empty") { CHECK_FALSE(status.bluetooth.has_value()); }
    }

    WHEN("internet transport connection fails")
    {
      const events::transport::connect_failed event{ .url = "wss://bad-relay.example.com",
        .error_message = "Connection timeout",
        .type = events::transport_type::internet };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("internet state shows disconnected with error")
      {
        REQUIRE(status.internet.has_value());
        if (status.internet.has_value()) {
          const auto &state = *status.internet;
          CHECK_FALSE(state.connected);
          CHECK(state.url == "wss://bad-relay.example.com");
          CHECK(state.error == "Connection timeout");
          CHECK(state.timestamp > 0);
        }
      }
    }

    WHEN("internet transport disconnects")
    {
      const events::transport::connected connect_event{ .url = "wss://relay.damus.io",
        .type = events::transport_type::internet };
      monitor.handle(connect_event);

      const events::transport::disconnected disconnect_event{ .type = events::transport_type::internet };
      monitor.handle(disconnect_event);

      auto status = monitor.get_status();

      THEN("internet state shows disconnected")
      {
        REQUIRE(status.internet.has_value());
        if (status.internet.has_value()) {
          const auto &state = *status.internet;
          CHECK_FALSE(state.connected);
          CHECK(state.error.empty());
        }
      }
    }
  }
}

SCENARIO("connection_monitor tracks bluetooth transport connection state", "[connection_monitor][bluetooth]")
{
  GIVEN("A connection_monitor instance")
  {
    const connection_monitor::out_queues_t queues{ .display = nullptr };
    connection_monitor monitor(queues);

    WHEN("bluetooth transport connects successfully")
    {
      const events::transport::connected event{ .url = "ble://device-123", .type = events::transport_type::bluetooth };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("bluetooth state shows connected")
      {
        REQUIRE(status.bluetooth.has_value());
        if (status.bluetooth.has_value()) {
          const auto &state = *status.bluetooth;
          CHECK(state.connected);
          CHECK(state.url == "ble://device-123");
          CHECK(state.error.empty());
          CHECK(state.timestamp > 0);
        }
      }

      THEN("internet state remains empty") { CHECK_FALSE(status.internet.has_value()); }
    }
  }
}

SCENARIO("connection_monitor handles both transports simultaneously", "[connection_monitor][hybrid]")
{
  GIVEN("A connection_monitor instance")
  {
    const connection_monitor::out_queues_t queues{ .display = nullptr };
    connection_monitor monitor(queues);

    WHEN("both internet and bluetooth connect")
    {
      const events::transport::connected internet_event{ .url = "wss://relay.damus.io",
        .type = events::transport_type::internet };
      const events::transport::connected bluetooth_event{ .url = "ble://device-123",
        .type = events::transport_type::bluetooth };

      monitor.handle(internet_event);
      monitor.handle(bluetooth_event);

      auto status = monitor.get_status();

      THEN("both states show connected")
      {
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
    }
  }
}

SCENARIO("connection_monitor clears error state on successful connection", "[connection_monitor][error_recovery]")
{
  GIVEN("A connection_monitor with a failed connection")
  {
    const connection_monitor::out_queues_t queues{ .display = nullptr };
    connection_monitor monitor(queues);

    const events::transport::connect_failed fail_event{ .url = "wss://bad-relay.example.com",
      .error_message = "DNS resolution failed",
      .type = events::transport_type::internet };
    monitor.handle(fail_event);

    WHEN("transport connects successfully")
    {
      const events::transport::connected success_event{ .url = "wss://good-relay.example.com",
        .type = events::transport_type::internet };
      monitor.handle(success_event);

      auto status = monitor.get_status();

      THEN("error is cleared")
      {
        REQUIRE(status.internet.has_value());
        if (status.internet.has_value()) {
          const auto &state = *status.internet;
          CHECK(state.connected);
          CHECK(state.error.empty());
          CHECK(state.url == "wss://good-relay.example.com");
        }
      }
    }
  }
}

SCENARIO("connection_monitor handles send_failed events", "[connection_monitor][send]")
{
  GIVEN("A connection_monitor with connected internet transport")
  {
    const connection_monitor::out_queues_t queues{ .display = nullptr };
    connection_monitor monitor(queues);

    const events::transport::connected connect_event{ .url = "wss://relay.damus.io",
      .type = events::transport_type::internet };
    monitor.handle(connect_event);

    WHEN("send fails")
    {
      const events::transport::send_failed fail_event{
        .message_id = "msg-456", .error_message = "Send timeout", .type = events::transport_type::internet
      };
      monitor.handle(fail_event);

      auto status = monitor.get_status();

      THEN("transport shows error but remains connected")
      {
        REQUIRE(status.internet.has_value());
        if (status.internet.has_value()) {
          const auto &state = *status.internet;
          CHECK(state.connected);
          CHECK(state.error == "Send timeout");
        }
      }
    }
  }
}

SCENARIO("connection_monitor responds to query_status by emitting display message", "[connection_monitor][query]")
{
  GIVEN("A connection_monitor with internet connected")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto display_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
    const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
    radix_relay::core::connection_monitor monitor(queues);

    const radix_relay::core::events::transport::connected connected_event{ .url = "wss://relay.example.com",
      .type = radix_relay::core::events::transport_type::internet };
    monitor.handle(connected_event);

    WHEN("query_status event is handled")
    {
      const radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message is pushed to queue with network status")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        if (msg.has_value()) {
          CHECK(msg->message.find("Network Status") != std::string::npos);
          CHECK(msg->message.find("Internet: Connected (wss://relay.example.com)") != std::string::npos);
          CHECK(msg->message.find("BLE Mesh:") != std::string::npos);
        }
      }
    }
  }

  GIVEN("A connection_monitor with internet disconnected")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto display_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
    const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
    radix_relay::core::connection_monitor monitor(queues);

    const radix_relay::core::events::transport::disconnected disconnected_event{
      .type = radix_relay::core::events::transport_type::internet
    };
    monitor.handle(disconnected_event);

    WHEN("query_status event is handled")
    {
      const radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message shows not connected status")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        if (msg.has_value()) { CHECK(msg->message.find("Internet: Not connected") != std::string::npos); }
      }
    }
  }

  GIVEN("A connection_monitor with failed connection")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto display_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
    const radix_relay::core::connection_monitor::out_queues_t queues{ .display = display_queue };
    radix_relay::core::connection_monitor monitor(queues);

    const radix_relay::core::events::transport::connect_failed failed_event{ .url = "wss://relay.fail.com",
      .error_message = "Connection timeout",
      .type = radix_relay::core::events::transport_type::internet };
    monitor.handle(failed_event);

    WHEN("query_status event is handled")
    {
      const radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message shows failed status with error")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        if (msg.has_value()) { CHECK(msg->message.find("Internet: Failed (Connection timeout)") != std::string::npos); }
      }
    }
  }
}
