include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(radix_relay_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(radix_relay_setup_options)
  option(radix_relay_ENABLE_HARDENING "Enable hardening" ON)
  option(radix_relay_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    radix_relay_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    radix_relay_ENABLE_HARDENING
    OFF)

  radix_relay_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR radix_relay_PACKAGING_MAINTAINER_MODE)
    option(radix_relay_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(radix_relay_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(radix_relay_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(radix_relay_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(radix_relay_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(radix_relay_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(radix_relay_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(radix_relay_ENABLE_PCH "Enable precompiled headers" OFF)
    option(radix_relay_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(radix_relay_ENABLE_IPO "Enable IPO/LTO" ON)
    option(radix_relay_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(radix_relay_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(radix_relay_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(radix_relay_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(radix_relay_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(radix_relay_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(radix_relay_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(radix_relay_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(radix_relay_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(radix_relay_ENABLE_PCH "Enable precompiled headers" OFF)
    option(radix_relay_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      radix_relay_ENABLE_IPO
      radix_relay_WARNINGS_AS_ERRORS
      radix_relay_ENABLE_USER_LINKER
      radix_relay_ENABLE_SANITIZER_ADDRESS
      radix_relay_ENABLE_SANITIZER_LEAK
      radix_relay_ENABLE_SANITIZER_UNDEFINED
      radix_relay_ENABLE_SANITIZER_THREAD
      radix_relay_ENABLE_SANITIZER_MEMORY
      radix_relay_ENABLE_UNITY_BUILD
      radix_relay_ENABLE_CLANG_TIDY
      radix_relay_ENABLE_CPPCHECK
      radix_relay_ENABLE_COVERAGE
      radix_relay_ENABLE_PCH
      radix_relay_ENABLE_CACHE)
  endif()

  radix_relay_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (radix_relay_ENABLE_SANITIZER_ADDRESS OR radix_relay_ENABLE_SANITIZER_THREAD OR radix_relay_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(radix_relay_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(radix_relay_global_options)
  if(radix_relay_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    radix_relay_enable_ipo()
  endif()

  radix_relay_supports_sanitizers()

  if(radix_relay_ENABLE_HARDENING AND radix_relay_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR radix_relay_ENABLE_SANITIZER_UNDEFINED
       OR radix_relay_ENABLE_SANITIZER_ADDRESS
       OR radix_relay_ENABLE_SANITIZER_THREAD
       OR radix_relay_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${radix_relay_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${radix_relay_ENABLE_SANITIZER_UNDEFINED}")
    radix_relay_enable_hardening(radix_relay_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(radix_relay_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(radix_relay_warnings INTERFACE)
  add_library(radix_relay_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  radix_relay_set_project_warnings(
    radix_relay_warnings
    ${radix_relay_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(radix_relay_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    radix_relay_configure_linker(radix_relay_options)
  endif()

  include(cmake/Sanitizers.cmake)
  radix_relay_enable_sanitizers(
    radix_relay_options
    ${radix_relay_ENABLE_SANITIZER_ADDRESS}
    ${radix_relay_ENABLE_SANITIZER_LEAK}
    ${radix_relay_ENABLE_SANITIZER_UNDEFINED}
    ${radix_relay_ENABLE_SANITIZER_THREAD}
    ${radix_relay_ENABLE_SANITIZER_MEMORY})

  set_target_properties(radix_relay_options PROPERTIES UNITY_BUILD ${radix_relay_ENABLE_UNITY_BUILD})

  if(radix_relay_ENABLE_PCH)
    target_precompile_headers(
      radix_relay_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(radix_relay_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    radix_relay_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(radix_relay_ENABLE_CLANG_TIDY)
    radix_relay_enable_clang_tidy(radix_relay_options ${radix_relay_WARNINGS_AS_ERRORS})
  endif()

  if(radix_relay_ENABLE_CPPCHECK)
    radix_relay_enable_cppcheck(${radix_relay_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(radix_relay_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    radix_relay_enable_coverage(radix_relay_options)
  endif()

  if(radix_relay_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(radix_relay_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(radix_relay_ENABLE_HARDENING AND NOT radix_relay_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR radix_relay_ENABLE_SANITIZER_UNDEFINED
       OR radix_relay_ENABLE_SANITIZER_ADDRESS
       OR radix_relay_ENABLE_SANITIZER_THREAD
       OR radix_relay_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    radix_relay_enable_hardening(radix_relay_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
