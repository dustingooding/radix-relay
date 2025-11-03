# Add library subdirectories
add_subdirectory(nostr)
add_subdirectory(signal)
add_subdirectory(transport)
add_subdirectory(platform)
add_subdirectory(core)
add_subdirectory(tui)

add_executable(radix-relay main.cpp)

target_link_libraries(
  radix-relay
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::tui
          radix_relay::core)

target_link_system_libraries(
  radix-relay
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog)

target_include_directories(radix-relay PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
