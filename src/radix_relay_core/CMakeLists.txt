include(GenerateExportHeader)


add_library(radix_relay_core cli.cpp)



add_library(radix_relay::radix_relay_core ALIAS radix_relay_core)

target_link_libraries(radix_relay_core PRIVATE radix_relay_options radix_relay_warnings)

target_link_system_libraries(
  radix_relay_core
  PRIVATE
          fmt::fmt)

target_include_directories(radix_relay_core ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                                     $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_include_directories(radix_relay_core PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

target_compile_features(radix_relay_core PUBLIC cxx_std_20)

set_target_properties(
  radix_relay_core
  PROPERTIES VERSION ${PROJECT_VERSION}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES)

generate_export_header(radix_relay_core EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/radix_relay/core_export.hpp)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(radix_relay_core PUBLIC RADIX_RELAY_CORE_STATIC_DEFINE)
endif()
