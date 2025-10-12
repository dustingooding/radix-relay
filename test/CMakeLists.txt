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
            radix_relay::radix_relay_core
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

add_catch_test(NAME cli_parser_integration_tests SOURCES cli_parser_integration_tests.cpp LIBS CLI11::CLI11;spdlog::spdlog)
add_catch_test(NAME cli_tests SOURCES cli_tests.cpp)
add_catch_test(NAME cli_utils_tests SOURCES cli_utils_tests.cpp LIBS CLI11::CLI11;fmt::fmt;spdlog::spdlog;signal_bridge_cxx)
add_catch_test(NAME command_handler_tests SOURCES command_handler_tests.cpp LIBS fmt::fmt)
add_catch_test(NAME event_handler_tests SOURCES event_handler_tests.cpp)
add_catch_test(NAME event_system_tests SOURCES event_system_tests.cpp)
add_catch_test(NAME node_identity_tests SOURCES node_identity_tests.cpp LIBS signal_bridge_cxx)
add_catch_test(NAME nostr_protocol_tests SOURCES nostr_protocol_tests.cpp LIBS nlohmann_json::nlohmann_json)
add_catch_test(NAME nostr_handler_tests SOURCES nostr_handler_tests.cpp LIBS nlohmann_json::nlohmann_json)
add_catch_test(NAME nostr_message_handler_tests SOURCES nostr_message_handler_tests.cpp LIBS signal_bridge_cxx Boost::asio nlohmann_json::nlohmann_json fmt::fmt)
add_catch_test(NAME session_orchestrator_tests SOURCES session_orchestrator_tests.cpp LIBS signal_bridge_cxx Boost::asio nlohmann_json::nlohmann_json fmt::fmt)
add_catch_test(NAME nostr_request_tracker_tests SOURCES nostr_request_tracker_tests.cpp LIBS nlohmann_json::nlohmann_json)
add_catch_test(NAME nostr_signing_tests SOURCES nostr_signing_tests.cpp LIBS nlohmann_json::nlohmann_json signal_bridge_cxx)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET cli_tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:cli_tests> $<TARGET_FILE_DIR:cli_tests>
    COMMAND_EXPAND_LISTS)
endif()

if(TARGET signal_bridge_cxx)
  add_catch_test(NAME signal_bridge_tests SOURCES signal_bridge_tests.cpp LIBS signal_bridge_cxx PREFIX signal)

  # Disable static analysis for generated CXX bridge code
  set_target_properties(signal_bridge_tests PROPERTIES
    CXX_CLANG_TIDY ""
    CXX_CPPCHECK "")
endif()
