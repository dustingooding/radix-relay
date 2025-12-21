#include <catch2/catch_test_macros.hpp>
#include <core/connection_monitor.hpp>
#include <core/events.hpp>

using namespace radix_relay::core;

SCENARIO("connection_monitor tracks internet transport connection state", "[connection_monitor][internet]")
{
  GIVEN("A connection_monitor instance")
  {
    connection_monitor monitor;

    WHEN("initially created")
    {
      auto status = monitor.get_status();

      THEN("both transport states are empty")
      {
        REQUIRE_FALSE(status.internet.has_value());
        REQUIRE_FALSE(status.bluetooth.has_value());
      }
    }

    WHEN("internet transport connects successfully")
    {
      events::transport::connected event{ .url = "wss://relay.damus.io", .type = transport_type::internet };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("internet state shows connected")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE(status.internet->connected);
        REQUIRE(status.internet->url == "wss://relay.damus.io");
        REQUIRE(status.internet->error.empty());
        REQUIRE(status.internet->timestamp > 0);
      }

      THEN("bluetooth state remains empty") { REQUIRE_FALSE(status.bluetooth.has_value()); }
    }

    WHEN("internet transport connection fails")
    {
      events::transport::connect_failed event{
        .url = "wss://bad-relay.example.com", .error_message = "Connection timeout", .type = transport_type::internet
      };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("internet state shows disconnected with error")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE_FALSE(status.internet->connected);
        REQUIRE(status.internet->url == "wss://bad-relay.example.com");
        REQUIRE(status.internet->error == "Connection timeout");
        REQUIRE(status.internet->timestamp > 0);
      }
    }

    WHEN("internet transport disconnects")
    {
      events::transport::connected connect_event{ .url = "wss://relay.damus.io", .type = transport_type::internet };
      monitor.handle(connect_event);

      events::transport::disconnected disconnect_event{ .type = transport_type::internet };
      monitor.handle(disconnect_event);

      auto status = monitor.get_status();

      THEN("internet state shows disconnected")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE_FALSE(status.internet->connected);
        REQUIRE(status.internet->error.empty());
      }
    }
  }
}

SCENARIO("connection_monitor tracks bluetooth transport connection state", "[connection_monitor][bluetooth]")
{
  GIVEN("A connection_monitor instance")
  {
    connection_monitor monitor;

    WHEN("bluetooth transport connects successfully")
    {
      events::transport::connected event{ .url = "ble://device-123", .type = transport_type::bluetooth };

      monitor.handle(event);
      auto status = monitor.get_status();

      THEN("bluetooth state shows connected")
      {
        REQUIRE(status.bluetooth.has_value());
        REQUIRE(status.bluetooth->connected);
        REQUIRE(status.bluetooth->url == "ble://device-123");
        REQUIRE(status.bluetooth->error.empty());
        REQUIRE(status.bluetooth->timestamp > 0);
      }

      THEN("internet state remains empty") { REQUIRE_FALSE(status.internet.has_value()); }
    }
  }
}

SCENARIO("connection_monitor handles both transports simultaneously", "[connection_monitor][hybrid]")
{
  GIVEN("A connection_monitor instance")
  {
    connection_monitor monitor;

    WHEN("both internet and bluetooth connect")
    {
      events::transport::connected internet_event{ .url = "wss://relay.damus.io", .type = transport_type::internet };
      events::transport::connected bluetooth_event{ .url = "ble://device-123", .type = transport_type::bluetooth };

      monitor.handle(internet_event);
      monitor.handle(bluetooth_event);

      auto status = monitor.get_status();

      THEN("both states show connected")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE(status.internet->connected);
        REQUIRE(status.internet->url == "wss://relay.damus.io");

        REQUIRE(status.bluetooth.has_value());
        REQUIRE(status.bluetooth->connected);
        REQUIRE(status.bluetooth->url == "ble://device-123");
      }
    }
  }
}

SCENARIO("connection_monitor clears error state on successful connection", "[connection_monitor][error_recovery]")
{
  GIVEN("A connection_monitor with a failed connection")
  {
    connection_monitor monitor;

    events::transport::connect_failed fail_event{
      .url = "wss://bad-relay.example.com", .error_message = "DNS resolution failed", .type = transport_type::internet
    };
    monitor.handle(fail_event);

    WHEN("transport connects successfully")
    {
      events::transport::connected success_event{ .url = "wss://good-relay.example.com",
        .type = transport_type::internet };
      monitor.handle(success_event);

      auto status = monitor.get_status();

      THEN("error is cleared")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE(status.internet->connected);
        REQUIRE(status.internet->error.empty());
        REQUIRE(status.internet->url == "wss://good-relay.example.com");
      }
    }
  }
}

SCENARIO("connection_monitor handles send_failed events", "[connection_monitor][send]")
{
  GIVEN("A connection_monitor with connected internet transport")
  {
    connection_monitor monitor;

    events::transport::connected connect_event{ .url = "wss://relay.damus.io", .type = transport_type::internet };
    monitor.handle(connect_event);

    WHEN("send fails")
    {
      events::transport::send_failed fail_event{
        .message_id = "msg-456", .error_message = "Send timeout", .type = transport_type::internet
      };
      monitor.handle(fail_event);

      auto status = monitor.get_status();

      THEN("transport shows error but remains connected")
      {
        REQUIRE(status.internet.has_value());
        REQUIRE(status.internet->connected);
        REQUIRE(status.internet->error == "Send timeout");
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
    radix_relay::core::connection_monitor monitor(display_queue);

    radix_relay::core::events::transport::connected connected_event{ .url = "wss://relay.example.com",
      .type = radix_relay::core::events::transport_type::internet };
    monitor.handle(connected_event);

    WHEN("query_status event is handled")
    {
      radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message is pushed to queue with network status")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        REQUIRE(msg->message.find("Network Status") != std::string::npos);
        REQUIRE(msg->message.find("Internet: Connected (wss://relay.example.com)") != std::string::npos);
        REQUIRE(msg->message.find("BLE Mesh:") != std::string::npos);
      }
    }
  }

  GIVEN("A connection_monitor with internet disconnected")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto display_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
    radix_relay::core::connection_monitor monitor(display_queue);

    radix_relay::core::events::transport::disconnected disconnected_event{
      .type = radix_relay::core::events::transport_type::internet
    };
    monitor.handle(disconnected_event);

    WHEN("query_status event is handled")
    {
      radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message shows not connected status")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        REQUIRE(msg->message.find("Internet: Not connected") != std::string::npos);
      }
    }
  }

  GIVEN("A connection_monitor with failed connection")
  {
    auto io_context = std::make_shared<boost::asio::io_context>();
    auto display_queue =
      std::make_shared<radix_relay::async::async_queue<radix_relay::core::events::display_message>>(io_context);
    radix_relay::core::connection_monitor monitor(display_queue);

    radix_relay::core::events::transport::connect_failed failed_event{ .url = "wss://relay.fail.com",
      .error_message = "Connection timeout",
      .type = radix_relay::core::events::transport_type::internet };
    monitor.handle(failed_event);

    WHEN("query_status event is handled")
    {
      radix_relay::core::events::connection_monitor::query_status query{};
      monitor.handle(query);

      THEN("display message shows failed status with error")
      {
        auto msg = display_queue->try_pop();
        REQUIRE(msg.has_value());
        REQUIRE(msg->message.find("Internet: Failed (Connection timeout)") != std::string::npos);
      }
    }
  }
}
