# signal_types library - Shared types between C++ and Rust Signal Protocol bridge

add_library(signal_types INTERFACE)
add_library(radix_relay::signal_types ALIAS signal_types)

target_include_directories(signal_types
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Depend on CXX-generated bridge headers
target_link_libraries(signal_types
  INTERFACE
    signal_bridge_cxx
)
