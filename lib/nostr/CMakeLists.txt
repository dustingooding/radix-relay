add_library(radix_relay_nostr
  src/protocol.cpp
  src/events.cpp
)

add_library(radix_relay::nostr ALIAS radix_relay_nostr)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_nostr
  PUBLIC
          radix_relay::core)

target_link_system_libraries(
  radix_relay_nostr
  PUBLIC
          nlohmann_json::nlohmann_json
          semver::semver
  PRIVATE
          fmt::fmt)

target_include_directories(radix_relay_nostr ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(radix_relay_nostr PUBLIC cxx_std_20)

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::nostr)
endif()
