add_executable(radix-relay radix_relay.cpp)

target_link_libraries(
  radix-relay
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::core
          radix_relay::nostr
          radix_relay::signal
          radix_relay::transport
          radix_relay::tui)

target_link_system_libraries(
  radix-relay
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog)

add_executable(echo_demo echo_demo.cpp)

target_link_libraries(
  echo_demo
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::core
          radix_relay::nostr
          radix_relay::platform
          radix_relay::signal
          radix_relay::transport)

target_link_system_libraries(
  echo_demo
  PRIVATE
          fmt::fmt
          spdlog::spdlog
          nlohmann_json::nlohmann_json)

if(MSVC)
  target_compile_options(echo_demo PRIVATE /bigobj)
endif()
