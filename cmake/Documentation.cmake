# Documentation build targets for Radix Relay
# Builds C++ (Doxygen), Rust (rustdoc), and narrative (MkDocs) documentation

# Find required tools
find_program(DOXYGEN_EXECUTABLE doxygen)
find_program(CARGO_EXECUTABLE cargo)
find_program(MKDOCS_EXECUTABLE mkdocs)

# C++ API documentation target (Doxygen)
if(DOXYGEN_EXECUTABLE)
  add_custom_target(
    docs-cpp
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating C++ API documentation with Doxygen"
    VERBATIM
  )

  # Copy Doxygen output to MkDocs site directory
  add_custom_target(
    docs-cpp-copy
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/out/site/api/cpp
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/out/doxygen/html
            ${CMAKE_SOURCE_DIR}/out/site/api/cpp
    DEPENDS docs-cpp
    COMMENT "Copying C++ API docs to site directory"
    VERBATIM
  )
else()
  message(STATUS "Doxygen not found. C++ API docs will not be built.")
  message(STATUS "Install with: spack install doxygen")

  add_custom_target(
    docs-cpp
    COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found. Install with: spack install doxygen"
    COMMENT "Doxygen not available"
  )

  add_custom_target(
    docs-cpp-copy
    DEPENDS docs-cpp
    COMMENT "Doxygen not available"
  )
endif()

# Rust API documentation target (rustdoc)
if(CARGO_EXECUTABLE)
  add_custom_target(
    docs-rust
    COMMAND ${CARGO_EXECUTABLE} doc --manifest-path ${CMAKE_SOURCE_DIR}/rust/Cargo.toml
            --no-deps --all-features
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Rust API documentation with rustdoc"
    VERBATIM
  )

  # Copy rustdoc output to MkDocs site directory
  add_custom_target(
    docs-rust-copy
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/out/site/api/rust
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/rust/target/doc
            ${CMAKE_SOURCE_DIR}/out/site/api/rust
    DEPENDS docs-rust
    COMMENT "Copying Rust API docs to site directory"
    VERBATIM
  )
else()
  message(STATUS "Cargo not found. Rust API docs will not be built.")

  add_custom_target(
    docs-rust
    COMMAND ${CMAKE_COMMAND} -E echo "Cargo not found. Rust API docs require cargo."
    COMMENT "Cargo not available"
  )

  add_custom_target(
    docs-rust-copy
    DEPENDS docs-rust
    COMMENT "Cargo not available"
  )
endif()

# Narrative documentation target (MkDocs)
if(MKDOCS_EXECUTABLE)
  add_custom_target(
    docs-mkdocs
    COMMAND ${MKDOCS_EXECUTABLE} build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building narrative documentation with MkDocs"
    VERBATIM
  )
else()
  message(STATUS "MkDocs not found. Narrative docs will not be built.")
  message(STATUS "Install with: pip install -r requirements.txt")

  add_custom_target(
    docs-mkdocs
    COMMAND ${CMAKE_COMMAND} -E echo "MkDocs not found. Install with: pip install -r requirements.txt"
    COMMENT "MkDocs not available"
  )
endif()

# Main documentation target - builds everything in the correct order
# 1. Build API docs (Doxygen and rustdoc)
# 2. Copy API docs to site directory
# 3. Build MkDocs with --dirty (preserves API docs)
add_custom_target(
  docs
  DEPENDS docs-cpp docs-rust
  COMMENT "Building complete documentation (C++ API, Rust API, and narrative docs)"
)

add_custom_command(
  TARGET docs POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/out/site/api/cpp
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/out/doxygen/html
          ${CMAKE_SOURCE_DIR}/out/site/api/cpp
  COMMENT "Copying C++ API docs to site"
  VERBATIM
)

add_custom_command(
  TARGET docs POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/out/site/api/rust
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/rust/target/doc
          ${CMAKE_SOURCE_DIR}/out/site/api/rust
  COMMENT "Copying Rust API docs to site"
  VERBATIM
)

add_custom_command(
  TARGET docs POST_BUILD
  COMMAND ${MKDOCS_EXECUTABLE} build --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Building MkDocs site (preserving API docs)"
  VERBATIM
)

# Add convenience target for serving docs locally
# Note: mkdocs serve doesn't work well with our copied API docs
# Use python http.server instead
add_custom_target(
  docs-serve
  DEPENDS docs
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "========================================"
  COMMAND ${CMAKE_COMMAND} -E echo "Documentation server starting..."
  COMMAND ${CMAKE_COMMAND} -E echo "Open http://127.0.0.1:8000/ in your browser"
  COMMAND ${CMAKE_COMMAND} -E echo "Press Ctrl+C to stop the server"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "If you get 'Address already in use', kill any"
  COMMAND ${CMAKE_COMMAND} -E echo "existing servers with: pkill -f 'http.server'"
  COMMAND ${CMAKE_COMMAND} -E echo "========================================"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND python3 -m http.server --directory ${CMAKE_SOURCE_DIR}/out/site 8000
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Serving documentation locally"
  VERBATIM
)

# Clean documentation output
add_custom_target(
  docs-clean
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/out/doxygen
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/out/site
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/rust/target/doc
  COMMENT "Cleaning documentation output directories"
  VERBATIM
)

message(STATUS "Documentation targets:")
message(STATUS "  docs          - Build all documentation")
message(STATUS "  docs-cpp      - Build C++ API documentation only")
message(STATUS "  docs-rust     - Build Rust API documentation only")
message(STATUS "  docs-mkdocs   - Build narrative documentation only")
if(MKDOCS_EXECUTABLE)
  message(STATUS "  docs-serve    - Serve documentation locally")
endif()
message(STATUS "  docs-clean    - Clean documentation output")
