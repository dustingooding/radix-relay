add_library(radix_relay_core INTERFACE)

add_library(radix_relay::radix_relay_core ALIAS radix_relay_core)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_core
  INTERFACE
          signal_bridge_cxx)

target_link_system_libraries(
  radix_relay_core
  INTERFACE
          fmt::fmt
          nlohmann_json::nlohmann_json
          Boost::asio
          OpenSSL::SSL
          OpenSSL::Crypto
          CLI11::CLI11
          spdlog::spdlog)

target_include_directories(radix_relay_core ${WARNING_GUARD} INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                                     $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
                                                                     $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_core INTERFACE cxx_std_20)
