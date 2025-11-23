cmake_minimum_required(VERSION 3.15...3.23)

project(RadixRelayBenchmarks LANGUAGES CXX)

# Signal Bridge Benchmarks
add_executable(signal_bridge_benchmark signal_bridge_benchmark.cpp)

target_link_libraries(
  signal_bridge_benchmark
  PRIVATE radix_relay::radix_relay_warnings
          radix_relay::radix_relay_options
          radix_relay::core
          Catch2::Catch2WithMain
          radix_relay::platform
          radix_relay::signal
          nlohmann_json::nlohmann_json)
