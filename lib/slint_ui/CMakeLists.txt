# Workaround: Slint's BUILD_RPATH doesn't propagate when used via CPM/FetchContent
# This ensures executables can find libslint_cpp.dylib at build time on macOS
# See: slint-ui/slint api/cpp/CMakeLists.txt lines 167-181
# Note: CACHE FORCE required so the value propagates to sibling directories (e.g., test/)
if(APPLE AND BUILD_SHARED_LIBS)
  set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH};${CMAKE_BINARY_DIR}/_deps/slint-build/api/cpp" CACHE STRING "" FORCE)
endif()

# Library containing only the Slint UI definitions and generated code
# This is marked as a SYSTEM library to exclude generated code from linting
add_library(radix_relay_slint_ui_gen)
add_library(radix_relay::slint_ui_gen ALIAS radix_relay_slint_ui_gen)

slint_target_sources(radix_relay_slint_ui_gen
  ui/main_window.slint
)

target_link_system_libraries(radix_relay_slint_ui_gen
  PUBLIC
    Slint::Slint
)

if(WIN32)
  target_link_libraries(radix_relay_slint_ui_gen PRIVATE
    dwrite         # DirectWrite
    propsys        # PropVariantToBSTR, VariantToPropVariant
    imm32          # IME functions
    dwmapi         # DWM window attributes
    comctl32       # Subclassing APIs
    uxtheme        # SetWindowTheme
  )
endif()

# Mark generated headers as SYSTEM to exclude from static analysis
target_include_directories(radix_relay_slint_ui_gen
  SYSTEM PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Disable static analysis tools for this target since it only contains generated code
set_target_properties(radix_relay_slint_ui_gen PROPERTIES
  CXX_CLANG_TIDY ""
  CXX_CPPCHECK ""
  CXX_INCLUDE_WHAT_YOU_USE ""
)

# Main library containing the processor implementation
add_library(radix_relay_slint_ui)
add_library(radix_relay::slint_ui ALIAS radix_relay_slint_ui)

target_sources(radix_relay_slint_ui
  PRIVATE
    src/processor.cpp
)

target_link_libraries(radix_relay_slint_ui
  PUBLIC
    radix_relay::radix_relay_options
    radix_relay::radix_relay_warnings
    radix_relay::core
    radix_relay::platform
)

target_link_system_libraries(radix_relay_slint_ui
  PUBLIC
    radix_relay::slint_ui_gen
    fmt::fmt
    spdlog::spdlog
)

target_include_directories(radix_relay_slint_ui
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(BUILD_TESTING)
  include(${PROJECT_SOURCE_DIR}/cmake/HeaderValidation.cmake)
  add_header_validation_targets(TARGET radix_relay::slint_ui)
endif()
