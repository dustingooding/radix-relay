cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

function(add_catch_test)
  cmake_parse_arguments(TEST_ARG "" "NAME;PREFIX" "SOURCES;LIBS" ${ARGN})

  if(NOT TEST_ARG_NAME)
    message(FATAL_ERROR "add_catch_test requires NAME")
  endif()

  if(NOT TEST_ARG_SOURCES)
    message(FATAL_ERROR "add_catch_test requires SOURCES")
  endif()

  if(NOT TEST_ARG_PREFIX)
    set(TEST_ARG_PREFIX "unittests")
  endif()

  add_executable(${TEST_ARG_NAME} ${TEST_ARG_SOURCES})

  target_link_libraries(
    ${TEST_ARG_NAME}
    PRIVATE radix_relay::radix_relay_warnings
            radix_relay::radix_relay_options
            radix_relay::core
            Catch2::Catch2WithMain
            ${TEST_ARG_LIBS})

  catch_discover_tests(
    ${TEST_ARG_NAME}
    TEST_PREFIX
    "${TEST_ARG_PREFIX}."
    REPORTER
    XML
    OUTPUT_DIR
    .
    OUTPUT_PREFIX
    "${TEST_ARG_PREFIX}."
    OUTPUT_SUFFIX
    .xml)
endfunction()

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(radix_relay CONFIG REQUIRED) # for intro, project_options, ...

  if(NOT TARGET radix_relay_options)
    message(FATAL_ERROR "Required config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# Include integration tests for the main CLI executable
include(main_cli_tests.cmake)

add_catch_test(NAME async_queue_tests SOURCES async_queue_tests.cpp)
add_catch_test(NAME cli_parser_integration_tests SOURCES cli_parser_integration_tests.cpp)
add_catch_test(NAME cli_tests SOURCES cli_tests.cpp)
add_catch_test(NAME cli_utils_tests SOURCES cli_utils_tests.cpp LIBS radix_relay::platform;radix_relay::signal)
add_catch_test(NAME command_handler_tests SOURCES command_handler_tests.cpp LIBS radix_relay::platform;radix_relay::signal;nlohmann_json::nlohmann_json)
add_catch_test(NAME command_processor_tests SOURCES command_processor_tests.cpp)
add_catch_test(NAME event_handler_tests SOURCES event_handler_tests.cpp)
add_catch_test(NAME event_system_tests SOURCES event_system_tests.cpp)
add_catch_test(NAME node_identity_tests SOURCES node_identity_tests.cpp LIBS radix_relay::platform;radix_relay::signal)
add_catch_test(NAME nostr_message_handler_tests SOURCES nostr_message_handler_tests.cpp LIBS radix_relay::nostr;radix_relay::signal)
add_catch_test(NAME nostr_protocol_tests SOURCES nostr_protocol_tests.cpp LIBS radix_relay::nostr)
add_catch_test(NAME nostr_request_tracker_tests SOURCES nostr_request_tracker_tests.cpp LIBS radix_relay::nostr)
add_catch_test(NAME nostr_signing_tests SOURCES nostr_signing_tests.cpp LIBS radix_relay::nostr;radix_relay::platform;radix_relay::signal)
add_catch_test(NAME nostr_transport_tests SOURCES nostr_transport_tests.cpp LIBS radix_relay::nostr;radix_relay::transport)
add_catch_test(NAME raw_signal_bridge_tests SOURCES raw_signal_bridge_tests.cpp LIBS radix_relay::platform;signal_bridge_cxx PREFIX signal)
add_catch_test(NAME scroll_state_tests SOURCES scroll_state_tests.cpp LIBS radix_relay::tui)
add_catch_test(NAME session_orchestrator_tests SOURCES session_orchestrator_tests.cpp LIBS radix_relay::nostr;radix_relay::signal)
add_catch_test(NAME signal_bridge_tests SOURCES signal_bridge_tests.cpp LIBS radix_relay::platform;radix_relay::signal;nlohmann_json::nlohmann_json PREFIX signal)
add_catch_test(NAME transport_event_display_handler_tests SOURCES transport_event_display_handler_tests.cpp)
add_catch_test(NAME transport_event_processor_tests SOURCES transport_event_processor_tests.cpp)
add_catch_test(NAME tui_processor_tests SOURCES tui_processor_tests.cpp LIBS radix_relay::tui)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET cli_tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:cli_tests> $<TARGET_FILE_DIR:cli_tests>
    COMMAND_EXPAND_LISTS)
endif()

# Disable static analysis for generated CXX bridge code
set_target_properties(signal_bridge_tests PROPERTIES
  CXX_CLANG_TIDY ""
  CXX_CPPCHECK "")

# Integration test for beast_websocket_stream (not in ALL target, requires network)
# Build with: cmake --build <build-dir> --target beast_websocket_integration_test
# Run with: ./test/beast_websocket_integration_test
add_executable(beast_websocket_integration_test EXCLUDE_FROM_ALL beast_websocket_integration_test.cpp)
target_link_libraries(
  beast_websocket_integration_test
  PRIVATE radix_relay::radix_relay_warnings
          radix_relay::radix_relay_options
          radix_relay::transport
          Catch2::Catch2WithMain)
