add_executable(radix-relay radix_relay.cpp)

target_link_libraries(
  radix-relay
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::core
          radix_relay::nostr
          radix_relay::signal
          radix_relay::transport
          radix_relay::tui
          radix_relay::gui)

target_link_system_libraries(
  radix-relay
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog)

if(MSVC)
  target_compile_options(radix-relay PRIVATE /bigobj)
endif()
