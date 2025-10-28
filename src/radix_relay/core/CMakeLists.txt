# Core is now an INTERFACE library that aggregates everything
add_library(radix_relay_core INTERFACE)

add_library(radix_relay::core ALIAS radix_relay_core)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

# Link all compiled sub-libraries
target_link_libraries(
  radix_relay_core
  INTERFACE
          radix_relay::nostr_protocol
          radix_relay::signal
          radix_relay::transport
          radix_relay::platform)

# Add remaining dependencies needed by header-only templates
target_link_system_libraries(
  radix_relay_core
  INTERFACE
          fmt::fmt
          CLI11::CLI11
          spdlog::spdlog)

target_include_directories(radix_relay_core ${WARNING_GUARD} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_core INTERFACE cxx_std_20)
