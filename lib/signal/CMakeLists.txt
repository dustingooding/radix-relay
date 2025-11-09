add_library(radix_relay_signal
  src/signal_bridge.cpp
  src/node_identity.cpp
)

add_library(radix_relay::signal ALIAS radix_relay_signal)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_signal
  PUBLIC
          signal_bridge_cxx
          radix_relay::core)

target_include_directories(radix_relay_signal ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(radix_relay_signal PUBLIC cxx_std_20)

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::signal)
endif()
