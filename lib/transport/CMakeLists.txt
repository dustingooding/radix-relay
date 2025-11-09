add_library(radix_relay_transport
  src/websocket_stream.cpp)

add_library(radix_relay::transport ALIAS radix_relay_transport)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_libraries(
  radix_relay_transport
  PUBLIC
          radix_relay::core)

target_link_system_libraries(
  radix_relay_transport
  PUBLIC
          Boost::asio
          Boost::beast
          Boost::system
          OpenSSL::SSL
          OpenSSL::Crypto)

target_include_directories(radix_relay_transport ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(radix_relay_transport PUBLIC cxx_std_20)

# Add /bigobj for MSVC to handle large object files from Boost.Beast templates
if(MSVC)
  target_compile_options(radix_relay_transport PRIVATE /bigobj)
endif()

# Header validation
if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::transport)
endif()
