# Rust Integration via Corrosion
# This file handles the setup and configuration of Rust compilation within CMake

function(setup_rust_workspace)
    find_program(RUST_CARGO cargo REQUIRED)
    find_program(RUST_RUSTC rustc REQUIRED)

    if(RUST_CARGO AND RUST_RUSTC)
        message(STATUS "Found Rust toolchain:")
        execute_process(COMMAND ${RUST_RUSTC} --version OUTPUT_VARIABLE RUSTC_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${RUST_CARGO} --version OUTPUT_VARIABLE CARGO_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "  ${RUSTC_VERSION}")
        message(STATUS "  ${CARGO_VERSION}")
        message(STATUS "  Using toolchain: ${Rust_TOOLCHAIN}")
    else()
        message(FATAL_ERROR "Rust toolchain not found. Please install Rust: https://rustup.rs/")
    endif()

    corrosion_import_crate(
        MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        PROFILE "release"
        CRATES test_crate
    )

    set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/../rust")
    corrosion_set_env_vars(test_crate "CARGO_TARGET_DIR=${RUST_TARGET_DIR}")

    add_test(
        NAME rust_tests
        COMMAND ${RUST_CARGO} test --manifest-path "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    set_tests_properties(rust_tests PROPERTIES
        ENVIRONMENT "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
    )

    message(STATUS "Rust workspace imported successfully")
    message(STATUS "  Rust target directory: ${RUST_TARGET_DIR}")
    message(STATUS "  Rust tests added to CTest")

endfunction()
