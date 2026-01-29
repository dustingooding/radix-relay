// Integration test for BLE stream with SimpleBLE/BlueZ stack
// Not included in default test suite - build manually when needed
//
// Usage:
//   cmake --build out/build/unixlike-clang-debug --target simpleble_bluez_integration_test
//   ./out/build/unixlike-clang-debug/test/simpleble_bluez_integration_test
//
// Linux Setup (BlueZ virtual HCI):
//   # Install tools
//   sudo apt-get install bluez-tools
//
//   # Create virtual HCI device
//   sudo modprobe vhci
//   sudo hciconfig hci0 up  # or hci1, etc.
//
//   # Verify device exists
//   hciconfig
//
// This test validates ble_stream integration with real BLE hardware

#include <transport/ble_stream.hpp>

#include <simpleble/SimpleBLE.h>

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace {
constexpr int scan_duration_ms = 5000;
constexpr int scan_lifecycle_delay_ms = 500;
constexpr int max_adapter_enum_attempts = 5;
constexpr int adapter_enum_delay_increment_ms = 100;
}// namespace

TEST_CASE("SimpleBLE can enumerate adapters", "[integration][ble][linux]")
{
  std::vector<SimpleBLE::Adapter> adapters;

  // Try multiple times with increasing delays
  for (int attempt = 0; attempt < max_adapter_enum_attempts && adapters.empty(); ++attempt) {
    if (attempt > 0) {
      std::cout << "Attempt " << (attempt + 1) << " after " << (attempt * adapter_enum_delay_increment_ms)
                << "ms delay\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(attempt * adapter_enum_delay_increment_ms));
    }

    std::cout << "Bluetooth enabled: " << SimpleBLE::Adapter::bluetooth_enabled() << '\n';

    try {
      adapters = SimpleBLE::Adapter::get_adapters();
      std::cout << "Found " << adapters.size() << " adapters\n";
    } catch (const std::exception &e) {
      std::cout << "Exception in get_adapters: " << e.what() << '\n';
      throw;
    }
  }

  INFO("Found " << adapters.size() << " BLE adapter(s)");

  if (adapters.empty()) {
    WARN("No BLE adapters found - test skipped");
    WARN("On Linux, ensure BlueZ is installed and hci device is up");
    return;
  }

  REQUIRE(!adapters.empty());

  for (size_t i = 0; i < adapters.size(); i++) {
    std::cout << "Adapter " << i << ": " << adapters[i].identifier() << " [" << adapters[i].address() << "]" << '\n';
  }
}

TEST_CASE("SimpleBLE can scan for BLE devices", "[integration][ble][linux]")
{
  auto adapters = SimpleBLE::Adapter::get_adapters();

  if (adapters.empty()) {
    WARN("No BLE adapters found - test skipped");
    return;
  }

  auto &adapter = adapters[0];
  INFO("Using adapter: " << adapter.identifier());

  std::vector<SimpleBLE::Peripheral> peripherals;

  adapter.set_callback_on_scan_found([&peripherals](SimpleBLE::Peripheral peripheral) {
    std::cout << "Found device: " << peripheral.identifier() << " [" << peripheral.address() << "]" << '\n';
    peripherals.push_back(peripheral);
  });

  adapter.scan_for(scan_duration_ms);

  INFO("Found " << peripherals.size() << " BLE device(s) during scan");

  if (peripherals.empty()) {
    WARN("No BLE devices found during scan - this is not necessarily a failure");
    WARN("Ensure there is a BLE device advertising nearby, or use virtual HCI peripheral");
  }

  // Test passes if scan completes without error, even if no devices found
  REQUIRE(true);
}

TEST_CASE("SimpleBLE adapter can start and stop scanning", "[integration][ble][linux]")
{
  auto adapters = SimpleBLE::Adapter::get_adapters();

  if (adapters.empty()) {
    WARN("No BLE adapters found - test skipped");
    return;
  }

  auto &adapter = adapters[0];

  bool scan_started = false;
  bool scan_stopped = false;

  adapter.set_callback_on_scan_start([&scan_started]() {
    scan_started = true;
    std::cout << "Scan started" << '\n';
  });

  adapter.set_callback_on_scan_stop([&scan_stopped]() {
    scan_stopped = true;
    std::cout << "Scan stopped" << '\n';
  });

  adapter.scan_start();
  std::this_thread::sleep_for(std::chrono::milliseconds(scan_lifecycle_delay_ms));
  adapter.scan_stop();

  REQUIRE(scan_started);
  REQUIRE(scan_stopped);
}

TEST_CASE("ble_stream can be constructed and has default MTU", "[integration][ble][stream]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::transport::ble_stream stream(io_context);

  REQUIRE(stream.get_mtu() == 20);
}

TEST_CASE("ble_stream async_connect handles no adapter gracefully", "[integration][ble][stream]")
{
  auto adapters = SimpleBLE::Adapter::get_adapters();

  if (not adapters.empty()) { WARN("BLE adapter found - test may not verify no-adapter path"); }

  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::transport::ble_stream stream(io_context);

  const radix_relay::transport::ble_connection_params params{ .device_address = "FF:FF:FF:FF:FF:FF",
    .service_uuid = "00001234-0000-1000-8000-00805f9b34fb",
    .characteristic_uuid = "00005678-0000-1000-8000-00805f9b34fb" };

  bool callback_invoked = false;
  boost::system::error_code received_error;

  stream.async_connect(params, [&](const boost::system::error_code &error, std::size_t) {
    callback_invoked = true;
    received_error = error;
    std::cout << "Connection result: " << (error ? error.message() : "success") << '\n';
  });

  io_context->run();

  REQUIRE(callback_invoked);

  if (adapters.empty()) {
    INFO("No adapter available - connection should fail");
    REQUIRE(received_error == boost::asio::error::connection_refused);
  } else {
    INFO("Adapter available but device not found - connection should fail");
    REQUIRE(received_error);
  }
}

TEST_CASE("ble_stream async_close always succeeds", "[integration][ble][stream]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::transport::ble_stream stream(io_context);

  bool callback_invoked = false;
  boost::system::error_code received_error;

  stream.async_close([&](const boost::system::error_code &error, std::size_t) {
    callback_invoked = true;
    received_error = error;
  });

  io_context->run();

  REQUIRE(callback_invoked);
  REQUIRE_FALSE(received_error);
}

TEST_CASE("ble_stream operations fail gracefully when not connected", "[integration][ble][stream]")
{
  auto io_context = std::make_shared<boost::asio::io_context>();
  radix_relay::transport::ble_stream stream(io_context);

  SECTION("async_write fails when not connected")
  {
    std::array<std::byte, 10> data{};
    bool callback_invoked = false;
    boost::system::error_code received_error;

    stream.async_write(std::span<const std::byte>(data), [&](const boost::system::error_code &error, std::size_t) {
      callback_invoked = true;
      received_error = error;
    });

    io_context->run();

    REQUIRE(callback_invoked);
    REQUIRE(received_error == boost::asio::error::not_connected);
  }

  SECTION("async_read fails when not connected")
  {
    std::array<std::byte, 100> buffer{};
    bool callback_invoked = false;
    boost::system::error_code received_error;

    stream.async_read(boost::asio::buffer(buffer), [&](const boost::system::error_code &error, std::size_t) {
      callback_invoked = true;
      received_error = error;
    });

    io_context->run();

    REQUIRE(callback_invoked);
    REQUIRE(received_error == boost::asio::error::not_connected);
  }
}
