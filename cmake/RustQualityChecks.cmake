# Rust Quality Checks Integration
# This file provides functions for integrating Rust quality tools (rustfmt, clippy) with CMake

macro(radix_relay_enable_rustfmt RUST_SOURCE_DIR WARNINGS_AS_ERRORS)
    find_program(RUST_CARGO cargo REQUIRED)
    find_program(RUSTFMT rustfmt)
    if(RUST_CARGO AND RUSTFMT)
        message(STATUS "Found rustfmt: ${RUSTFMT}")

        # Create a target for formatting check
        add_custom_target(rust-fmt-check
            COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/../rust ${RUST_CARGO} fmt --manifest-path "${RUST_SOURCE_DIR}/Cargo.toml" --all --check
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Checking Rust code formatting with rustfmt"
        )

        # Create a target for applying formatting
        add_custom_target(rust-fmt
            COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/../rust ${RUST_CARGO} fmt --manifest-path "${RUST_SOURCE_DIR}/Cargo.toml" --all
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Formatting Rust code with rustfmt"
        )

        # Add to quality checks group
        if(NOT TARGET quality-rust)
            add_custom_target(quality-rust)
        endif()
        add_dependencies(quality-rust rust-fmt-check)

    else()
        message(WARNING "rustfmt requested but executable not found")
    endif()
endmacro()

macro(radix_relay_enable_clippy RUST_SOURCE_DIR WARNINGS_AS_ERRORS)
    find_program(CARGO cargo REQUIRED)
    if(CARGO)
        message(STATUS "Found cargo: ${CARGO}")

        # Check if clippy is available
        execute_process(
            COMMAND ${CARGO} clippy --version
            RESULT_VARIABLE CLIPPY_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(CLIPPY_RESULT EQUAL 0)
            message(STATUS "Clippy is available")

            # Set clippy arguments based on warnings as errors setting
            if(${WARNINGS_AS_ERRORS})
                set(CLIPPY_ARGS "clippy" "--manifest-path" "${RUST_SOURCE_DIR}/Cargo.toml" "--all-targets" "--" "-D" "warnings")
                set(CLIPPY_COMMENT "Running Clippy linter (warnings as errors)")
            else()
                set(CLIPPY_ARGS "clippy" "--manifest-path" "${RUST_SOURCE_DIR}/Cargo.toml" "--all-targets")
                set(CLIPPY_COMMENT "Running Clippy linter")
            endif()

            # Create clippy target
            add_custom_target(rust-clippy
                COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/../rust ${CARGO} ${CLIPPY_ARGS}
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                COMMENT "${CLIPPY_COMMENT}"
            )

            # Add to quality checks group
            if(NOT TARGET quality-rust)
                add_custom_target(quality-rust)
            endif()
            add_dependencies(quality-rust rust-clippy)

        else()
            message(WARNING "clippy requested but not available. Install with: rustup component add clippy")
        endif()
    else()
        message(WARNING "cargo not found, cannot enable clippy")
    endif()
endmacro()

macro(radix_relay_setup_rust_quality_checks RUST_SOURCE_DIR)
    # Default to warnings as errors in developer mode
    if(radix_relay_PACKAGING_MAINTAINER_MODE)
        set(RUST_WARNINGS_AS_ERRORS ON)
    else()
        set(RUST_WARNINGS_AS_ERRORS OFF)
    endif()

    radix_relay_enable_rustfmt("${RUST_SOURCE_DIR}" ${RUST_WARNINGS_AS_ERRORS})
    radix_relay_enable_clippy("${RUST_SOURCE_DIR}" ${RUST_WARNINGS_AS_ERRORS})

    message(STATUS "Rust quality checks configured (warnings as errors: ${RUST_WARNINGS_AS_ERRORS})")
endmacro()
