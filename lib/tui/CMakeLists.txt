add_library(radix_relay_tui INTERFACE)

add_library(radix_relay::tui ALIAS radix_relay_tui)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_tui
  INTERFACE
          radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::core
)

target_link_system_libraries(
  radix_relay_tui
  INTERFACE
          replxx::replxx
          fmt::fmt
)

target_include_directories(radix_relay_tui INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::tui)
endif()
