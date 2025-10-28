add_library(radix_relay_signal
  signal_bridge.cpp
  node_identity.cpp
)

add_library(radix_relay::signal ALIAS radix_relay_signal)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_signal
  PUBLIC
          signal_bridge_cxx)

target_include_directories(radix_relay_signal ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_signal PUBLIC cxx_std_20)
