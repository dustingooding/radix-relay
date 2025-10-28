add_library(radix_relay_transport
  beast_websocket_stream.cpp
  uuid_generator.cpp
)

add_library(radix_relay::transport ALIAS radix_relay_transport)

include(${PROJECT_SOURCE_DIR}/cmake/SystemLink.cmake)

target_link_system_libraries(
  radix_relay_transport
  PUBLIC
          Boost::asio
          Boost::beast
          Boost::system
          OpenSSL::SSL
          OpenSSL::Crypto
  PRIVATE
          Boost::uuid)

target_include_directories(radix_relay_transport ${WARNING_GUARD} PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/configured_files/include>)

target_compile_features(radix_relay_transport PUBLIC cxx_std_20)

# Add /bigobj for MSVC to handle large object files from Boost.Beast templates
if(MSVC)
  target_compile_options(radix_relay_transport PRIVATE /bigobj)
endif()
