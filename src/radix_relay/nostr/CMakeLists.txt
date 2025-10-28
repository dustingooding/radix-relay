add_library(radix_relay_nostr_protocol
  protocol.cpp
  events.cpp
)

add_library(radix_relay::nostr_protocol ALIAS radix_relay_nostr_protocol)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_system_libraries(
  radix_relay_nostr_protocol
  PUBLIC
          nlohmann_json::nlohmann_json
  PRIVATE
          fmt::fmt)

target_include_directories(radix_relay_nostr_protocol ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_nostr_protocol PUBLIC cxx_std_20)
