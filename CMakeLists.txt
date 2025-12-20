cmake_minimum_required(VERSION 3.21)

# This template attempts to be "fetch_content"-able
# so that it works well with tools like CPM or other
# manual dependency management

# Only set the cxx_standard if it is not set by someone else
if (NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

# Set macOS deployment target to match libsignal requirement
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
endif()

# Set Windows target version to Windows 7 for Boost ASIO
if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable PIC for all targets (required for shared libraries on Unix)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Shared libs on Windows are terrible.
if(WIN32)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

# Read version from VERSION file
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PROJECT_VERSION_FROM_FILE)
string(STRIP "${PROJECT_VERSION_FROM_FILE}" PROJECT_VERSION_FROM_FILE)

# Set the project name and language
project(
  radix_relay
  VERSION ${PROJECT_VERSION_FROM_FILE}
  DESCRIPTION ""
  HOMEPAGE_URL "https://github.com/dustingooding/radix-relay"
  LANGUAGES CXX C)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)


radix_relay_setup_options()

radix_relay_global_options()
include(Dependencies.cmake)
radix_relay_setup_dependencies()

# Rust integration via Corrosion
include(cmake/RustIntegration.cmake)
setup_rust_workspace()

radix_relay_local_options()

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)

target_compile_features(radix_relay_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(radix_relay::radix_relay_options ALIAS radix_relay_options)
add_library(radix_relay::radix_relay_warnings ALIAS radix_relay_warnings)

#add_library(radix_relay::radix_relay_options INTERFACE IMPORTED)
#add_library(radix_relay::radix_relay_warnings INTERFACE IMPORTED)

# Add library subdirectories
add_subdirectory(lib/nostr)
add_subdirectory(lib/signal)
add_subdirectory(lib/transport)
add_subdirectory(lib/platform)
add_subdirectory(lib/core)
add_subdirectory(lib/tui)
add_subdirectory(lib/gui)

# Adding the src:
add_subdirectory(src)

# Don't even look at tests if we're not top level
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

# Adding the tests:
include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

# Benchmarks
add_subdirectory(benchmarks)

# Code quality targets (clang-tidy, etc.)
include(cmake/Quality.cmake)

# Documentation target
include(cmake/Documentation.cmake)

if(radix_relay_BUILD_FUZZ_TESTS)
  message(AUTHOR_WARNING "Building Fuzz Tests, using fuzzing sanitizer https://www.llvm.org/docs/LibFuzzer.html")
  if (NOT radix_relay_ENABLE_SANITIZER_ADDRESS AND NOT radix_relay_ENABLE_SANITIZER_THREAD)
    message(WARNING "You need asan or tsan enabled for meaningful fuzz testing")
  endif()
  add_subdirectory(fuzz_test)
endif()

# If MSVC is being used, and ASAN is enabled, we need to set the debugger environment
# so that it behaves well with MSVC's debugger, and we can run the target from visual studio
if(MSVC)
  get_all_installable_targets(all_targets)
  message("all_targets=${all_targets}")
  set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$(VC_ExecutablePath_x64);%PATH%")
endif()

# set the startup project for the "play" button in MSVC
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT radix-relay)

if(CMAKE_SKIP_INSTALL_RULES)
  return()
endif()

include(cmake/PackageProject.cmake)

# Add other targets that you want installed here, by default we just package the one executable
# we know we want to ship
radix_relay_package_project(
  TARGETS
  radix-relay
  radix_relay_options
  radix_relay_warnings
  # FIXME: this does not work! CK
  # PRIVATE_DEPENDENCIES_CONFIGURED project_options project_warnings
)

# Experience shows that explicit package naming can help make it easier to sort
# out potential ABI related issues before they start, while helping you
# track a build to a specific GIT SHA
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
