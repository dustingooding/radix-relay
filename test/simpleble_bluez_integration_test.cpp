// Integration test for SimpleBLE library with BlueZ stack
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
// This test will scan for nearby BLE devices to validate SimpleBLE integration

#include <simpleble/SimpleBLE.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
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
