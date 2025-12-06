add_library(radix_relay_platform
  src/env_utils.cpp
  src/time_utils.cpp
)

add_library(radix_relay::platform ALIAS radix_relay_platform)

target_include_directories(radix_relay_platform ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(radix_relay_platform
  PUBLIC
    fmt::fmt
)

target_compile_features(radix_relay_platform PUBLIC cxx_std_20)

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::platform)
endif()
