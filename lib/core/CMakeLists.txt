# Configure version file
configure_file("configured_files/config.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/internal_use_only/config.hpp" ESCAPE_QUOTES)

# Core library with concepts, utilities, and event handling
add_library(radix_relay_core
  src/uuid_generator.cpp
  src/connection_monitor.cpp)

add_library(radix_relay::core ALIAS radix_relay_core)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

# Core provides concepts, async utilities, and event handling
# Depends on platform for low-level OS utilities
target_link_libraries(
  radix_relay_core
  PUBLIC
          radix_relay::platform)

target_link_system_libraries(
  radix_relay_core
  PUBLIC
          Boost::asio
          Boost::system
          Boost::uuid
          fmt::fmt
          CLI11::CLI11
          spdlog::spdlog
          semver::semver)

target_include_directories(radix_relay_core ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(radix_relay_core PUBLIC cxx_std_20)

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::core)
endif()
