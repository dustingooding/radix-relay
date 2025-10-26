#include "test_doubles/test_double_websocket_stream.hpp"
#include <radix_relay/nostr_transport.hpp>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <vector>

namespace radix_relay::nostr::test {

SCENARIO("Templated nostr transport uses WebSocket stream via concept", "[nostr][transport]")
{
  GIVEN("An io_context and fake WebSocket stream")
  {
    boost::asio::io_context io_context;
    auto fake = std::make_shared<radix_relay::test::test_double_websocket_stream>(io_context);

    WHEN("Transport is constructed with the fake and callback")
    {
      bool bytes_received = false;
      std::vector<std::byte> received_bytes;

      const transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, [&](std::vector<std::byte> bytes) {
          bytes_received = true;
          received_bytes = std::move(bytes);
        });

      THEN("Transport is created successfully") { REQUIRE(true); }
    }

    WHEN("Transport connects to a URL")
    {
      transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, [](const std::vector<std::byte> & /*bytes*/) {});

      bool connected = false;
      transport.async_connect(
        "wss://relay.damus.io", [&connected](const boost::system::error_code & /*ec*/) { connected = true; });

      while (!connected) { io_context.poll_one(); }

      THEN("WebSocket stream receives connection request")
      {
        const auto &connections = fake->get_connections();
        REQUIRE(connections.size() == 1);
        REQUIRE(connections[0].host == "relay.damus.io");
        REQUIRE(connections[0].port == "443");
        REQUIRE(connections[0].path == "/");
      }
    }

    WHEN("Transport sends data after connecting")
    {
      transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, [](const std::vector<std::byte> & /*bytes*/) {});

      bool connected = false;
      transport.async_connect(
        "wss://relay.damus.io", [&connected](const boost::system::error_code & /*ec*/) { connected = true; });

      while (!connected) { io_context.poll_one(); }

      std::vector<std::byte> data{ std::byte{ 0x01 }, std::byte{ 0x02 } };
      transport.send(data);
      io_context.restart();
      io_context.run();

      THEN("WebSocket stream receives write request")
      {
        const auto &writes = fake->get_writes();
        REQUIRE(writes.size() == 1);
        REQUIRE(writes[0].data == data);
      }
    }

    WHEN("Transport receives incoming data after connecting")
    {
      bool bytes_received = false;
      std::vector<std::byte> received_bytes;

      transport<radix_relay::test::test_double_websocket_stream> transport(
        fake, io_context, [&](std::vector<std::byte> bytes) {
          bytes_received = true;
          received_bytes = std::move(bytes);
        });

      bool connected = false;
      transport.async_connect(
        "wss://relay.damus.io", [&connected](const boost::system::error_code & /*ec*/) { connected = true; });

      while (!connected) { io_context.poll_one(); }

      constexpr std::byte test_byte_1{ 0xAB };
      constexpr std::byte test_byte_2{ 0xCD };
      constexpr std::byte test_byte_3{ 0xEF };
      std::vector<std::byte> incoming_data{ test_byte_1, test_byte_2, test_byte_3 };
      fake->set_read_data(incoming_data);
      io_context.restart();
      io_context.run();

      THEN("Transport invokes callback with received data")
      {
        REQUIRE(bytes_received);
        REQUIRE(received_bytes == incoming_data);
      }
    }
  }
}

}// namespace radix_relay::nostr::test
