# Quality.cmake
# Code quality and static analysis targets

find_program(CLANGD_TIDY clangd-tidy)
if(CLANGD_TIDY)
  message(STATUS "Found clangd-tidy: ${CLANGD_TIDY}")

  file(GLOB_RECURSE PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
    ${CMAKE_SOURCE_DIR}/lib/*/include/*.hpp
    ${CMAKE_SOURCE_DIR}/lib/*/src/*.cpp
  )

  add_custom_target(quality
    COMMAND ${CLANGD_TIDY}
      -p ${CMAKE_BINARY_DIR}
      ${PROJECT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy on project sources"
    VERBATIM
  )

  # Depend on Rust bridge to ensure generated headers exist
  if(TARGET signal_bridge_cxx)
    add_dependencies(quality signal_bridge_cxx)
  endif()

  # Depend on header validation to ensure headers compile independently
  if(TARGET validate_headers)
    add_dependencies(quality validate_headers)
  endif()
else()
  message(STATUS "clangd-tidy not found - install with: pip install clangd-tidy")
endif()
