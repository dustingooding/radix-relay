add_subdirectory(radix_relay)

add_executable(echo_demo echo_demo.cpp)

target_link_libraries(
  echo_demo
  PRIVATE radix_relay::radix_relay_options
          radix_relay::radix_relay_warnings
          radix_relay::core)

target_link_system_libraries(
  echo_demo
  PRIVATE
          fmt::fmt
          spdlog::spdlog
          nlohmann_json::nlohmann_json)

target_include_directories(echo_demo PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")

if(MSVC)
  target_compile_options(echo_demo PRIVATE /bigobj)
endif()
