#include <transport/ble_stream.hpp>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace radix_relay::transport::test {

TEST_CASE("BLE stream can be constructed with io_context", "[ble][stream]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  const ble_stream stream(io_context);

  CHECK(stream.get_mtu() == 20);
}

TEST_CASE("BLE stream async_connect handles no adapter gracefully", "[ble][stream][connect]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  ble_stream stream(io_context);

  const ble_connection_params params{ .device_address = "AA:BB:CC:DD:EE:FF",
    .service_uuid = "00001234-0000-1000-8000-00805f9b34fb",
    .characteristic_uuid = "00005678-0000-1000-8000-00805f9b34fb" };

  bool callback_invoked = false;
  boost::system::error_code received_error;

  stream.async_connect(params, [&](const boost::system::error_code &error, std::size_t) {
    callback_invoked = true;
    received_error = error;
  });

  io_context->run();

  CHECK(callback_invoked);

  if (received_error) {
    INFO("No BLE adapter available - connection failed as expected");
    CHECK(received_error == boost::asio::error::connection_refused);
  }
}

TEST_CASE("BLE stream async_close succeeds without connection", "[ble][stream][disconnect]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  ble_stream stream(io_context);

  bool close_done = false;
  boost::system::error_code close_error;

  stream.async_close([&](const boost::system::error_code &error, std::size_t) {
    close_done = true;
    close_error = error;
  });

  io_context->run();

  CHECK(close_done);
  CHECK_FALSE(close_error);
}

TEST_CASE("BLE stream async_write fails when not connected", "[ble][stream][write]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  ble_stream stream(io_context);

  std::array<std::byte, 5> data{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 }, std::byte{ 4 }, std::byte{ 5 } };

  bool callback_invoked = false;
  boost::system::error_code received_error;

  stream.async_write(std::span<const std::byte>(data), [&](const boost::system::error_code &error, std::size_t) {
    callback_invoked = true;
    received_error = error;
  });

  io_context->run();

  CHECK(callback_invoked);
  CHECK(received_error == boost::asio::error::not_connected);
}

TEST_CASE("BLE stream async_read fails when not connected", "[ble][stream][read]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  ble_stream stream(io_context);

  std::array<std::byte, 100> buffer{};

  bool callback_invoked = false;
  boost::system::error_code received_error;

  stream.async_read(boost::asio::buffer(buffer), [&](const boost::system::error_code &error, std::size_t) {
    callback_invoked = true;
    received_error = error;
  });

  io_context->run();

  CHECK(callback_invoked);
  CHECK(received_error == boost::asio::error::not_connected);
}

}// namespace radix_relay::transport::test
