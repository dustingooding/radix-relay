cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

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

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND radix-relay --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND radix-relay --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

add_executable(cli_tests cli_tests.cpp)
target_link_libraries(
  cli_tests
  PRIVATE radix_relay::radix_relay_warnings
          radix_relay::radix_relay_options
          radix_relay::radix_relay_core
          Catch2::Catch2WithMain)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET cli_tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:cli_tests> $<TARGET_FILE_DIR:cli_tests>
    COMMAND_EXPAND_LISTS)
endif()

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  cli_tests
  TEST_PREFIX
  "unittests."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)

