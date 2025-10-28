add_library(radix_relay_platform
  env_utils.cpp
)

add_library(radix_relay::platform ALIAS radix_relay_platform)

target_include_directories(radix_relay_platform ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_platform PUBLIC cxx_std_20)
