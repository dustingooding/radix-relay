add_subdirectory(radix_relay)
add_subdirectory(radix_relay/core)

# Echo demonstration executable (strand-based architecture)
add_executable(echo_demo_strand echo_demo_strand.cpp)

target_link_libraries(
  echo_demo_strand
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::radix_relay_core)

target_link_system_libraries(
  echo_demo_strand
  PRIVATE
          fmt::fmt
          spdlog::spdlog
          nlohmann_json::nlohmann_json)

target_include_directories(echo_demo_strand PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

# Windows MSVC requires /bigobj for files with many template instantiations
if(MSVC)
  target_compile_options(echo_demo_strand PRIVATE /bigobj)
endif()
