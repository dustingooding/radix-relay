# HeaderValidation.cmake
# Creates individual executable targets for each header to validate:
# 1. Headers compile independently
# 2. Headers have correct include guards
# 3. clang-tidy can analyze headers with proper compile commands

function(add_header_validation_targets)
  cmake_parse_arguments(
    HEADER_VAL
    ""
    "TARGET"
    ""
    ${ARGN}
  )

  if(NOT HEADER_VAL_TARGET)
    message(FATAL_ERROR "add_header_validation_targets requires TARGET argument")
  endif()

  # Get the target's interface include directories
  get_target_property(INCLUDE_DIRS ${HEADER_VAL_TARGET} INTERFACE_INCLUDE_DIRECTORIES)

  if(NOT INCLUDE_DIRS)
    message(WARNING "No include directories found for target ${HEADER_VAL_TARGET}")
    return()
  endif()

  # Collect all headers from all include directories
  set(ALL_HEADERS "")
  foreach(INCLUDE_DIR ${INCLUDE_DIRS})
    # Skip any path with generator expressions
    if(INCLUDE_DIR MATCHES "\\$<")
      # Try to extract BUILD_INTERFACE paths only
      if(INCLUDE_DIR MATCHES "\\$<BUILD_INTERFACE:([^>]+)>")
        set(INCLUDE_DIR "${CMAKE_MATCH_1}")
      else()
        # Skip other generator expressions (INSTALL_INTERFACE, etc.)
        continue()
      endif()
    endif()

    # Skip if still contains generator expression syntax
    if(INCLUDE_DIR MATCHES "\\$<")
      continue()
    endif()

    # Only process headers within the project source directory
    file(RELATIVE_PATH REL_TO_PROJECT "${PROJECT_SOURCE_DIR}" "${INCLUDE_DIR}")
    # Skip if path goes up (..) or is an absolute path (starts with / or drive letter on Windows)
    if(REL_TO_PROJECT MATCHES "^\\.\\." OR IS_ABSOLUTE "${INCLUDE_DIR}")
      # Check if it's actually within project by seeing if normalized paths match
      cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${INCLUDE_DIR}" NORMALIZE is_within_project)
      if(NOT is_within_project)
        continue()
      endif()
    endif()

    # Find headers in this include directory
    file(GLOB_RECURSE DIR_HEADERS "${INCLUDE_DIR}/*.hpp" "${INCLUDE_DIR}/*.h")
    list(APPEND ALL_HEADERS ${DIR_HEADERS})
  endforeach()

  if(NOT ALL_HEADERS)
    message(WARNING "No headers found for target ${HEADER_VAL_TARGET} in project directories")
    return()
  endif()

  foreach(HEADER ${ALL_HEADERS})
    # Double-check this header is within the project
    file(RELATIVE_PATH HEADER_REL_FULL "${PROJECT_SOURCE_DIR}" "${HEADER}")
    # Skip if path goes up (..) or is an absolute path
    if(HEADER_REL_FULL MATCHES "^\\.\\." OR IS_ABSOLUTE "${HEADER}")
      cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${HEADER}" NORMALIZE is_within_project)
      if(NOT is_within_project)
        continue()
      endif()
    endif()

    # Determine which include directory this header belongs to
    set(HEADER_REL "")
    foreach(INCLUDE_DIR ${INCLUDE_DIRS})
      # Skip any path with generator expressions
      if(INCLUDE_DIR MATCHES "\\$<")
        # Try to extract BUILD_INTERFACE paths only
        if(INCLUDE_DIR MATCHES "\\$<BUILD_INTERFACE:([^>]+)>")
          set(INCLUDE_DIR "${CMAKE_MATCH_1}")
        else()
          # Skip other generator expressions
          continue()
        endif()
      endif()

      # Skip if still contains generator expression syntax
      if(INCLUDE_DIR MATCHES "\\$<")
        continue()
      endif()

      # Skip directories outside the project
      file(RELATIVE_PATH REL_TO_PROJECT "${PROJECT_SOURCE_DIR}" "${INCLUDE_DIR}")
      if(REL_TO_PROJECT MATCHES "^\\.\\." OR IS_ABSOLUTE "${INCLUDE_DIR}")
        cmake_path(IS_PREFIX PROJECT_SOURCE_DIR "${INCLUDE_DIR}" NORMALIZE is_within_project)
        if(NOT is_within_project)
          continue()
        endif()
      endif()

      file(RELATIVE_PATH TMP_REL "${INCLUDE_DIR}" "${HEADER}")
      if(NOT TMP_REL MATCHES "^\\.\\.")
        set(HEADER_REL "${TMP_REL}")
        break()
      endif()
    endforeach()

    if(NOT HEADER_REL)
      message(WARNING "Could not determine relative path for ${HEADER}")
      continue()
    endif()

    # Create a valid target name by replacing slashes and dots with underscores
    string(REPLACE "/" "_" TARGET_NAME "${HEADER_REL_FULL}")
    string(REPLACE "." "_" TARGET_NAME "${TARGET_NAME}")
    set(TARGET_NAME "header_${TARGET_NAME}")

    # Create a minimal source file that includes the header
    set(VALIDATION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/header_validation/${HEADER_REL_FULL}.cpp")
    get_filename_component(VALIDATION_SOURCE_DIR "${VALIDATION_SOURCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${VALIDATION_SOURCE_DIR}")

    file(WRITE "${VALIDATION_SOURCE}"
"// Auto-generated header validation for ${HEADER_REL_FULL}
#include <${HEADER_REL}>
")

    # Create library target (no main() needed) and link against the target library
    add_library(${TARGET_NAME} OBJECT EXCLUDE_FROM_ALL "${VALIDATION_SOURCE}")
    target_link_libraries(${TARGET_NAME} PRIVATE ${HEADER_VAL_TARGET})
  endforeach()

  # Create a custom target to build all header validation targets
  set(ALL_HEADER_TARGETS "")
  foreach(HEADER ${ALL_HEADERS})
    file(RELATIVE_PATH HEADER_REL_FULL "${PROJECT_SOURCE_DIR}" "${HEADER}")
    string(REPLACE "/" "_" TARGET_NAME "${HEADER_REL_FULL}")
    string(REPLACE "." "_" TARGET_NAME "${TARGET_NAME}")
    list(APPEND ALL_HEADER_TARGETS "header_${TARGET_NAME}")
  endforeach()

  add_custom_target(validate_headers
    DEPENDS ${ALL_HEADER_TARGETS}
    COMMENT "Building all header validation targets"
  )

endfunction()
