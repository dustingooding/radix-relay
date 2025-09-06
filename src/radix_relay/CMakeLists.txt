add_executable(radix-relay main.cpp)

target_link_libraries(
  radix-relay
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::radix_relay_core)

target_link_system_libraries(
  radix-relay
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog)

target_include_directories(radix-relay PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")