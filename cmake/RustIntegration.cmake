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

    # Configure Rust build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ENV{CARGO_BUILD_TYPE} "debug")
        message(STATUS "  Rust Debug build configured")
    else()
        set(ENV{CARGO_BUILD_TYPE} "release")
        message(STATUS "  Rust Release build configured")
    endif()

    corrosion_import_crate(
        MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        CRATES test_crate
    )

    # Add CXX bridge for C++ interoperability
    corrosion_add_cxxbridge(
        test_crate_cxx
        CRATE test_crate
        FILES lib.rs
    )

    # Configure CXX bridge C++ compilation to match CMake MSVC runtime library
    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            # Ensure CXX bridge C++ code uses debug MSVC runtime (/MDd)
            corrosion_set_env_vars(test_crate
                "CFLAGS=/MDd /D_ITERATOR_DEBUG_LEVEL=2"
                "CXXFLAGS=/MDd /D_ITERATOR_DEBUG_LEVEL=2"
            )
            message(STATUS "  CXX bridge: Using MSVC debug runtime (/MDd) with iterator debug level 2")
        else()
            # Ensure CXX bridge C++ code uses release MSVC runtime (/MD)
            corrosion_set_env_vars(test_crate
                "CFLAGS=/MD /D_ITERATOR_DEBUG_LEVEL=0"
                "CXXFLAGS=/MD /D_ITERATOR_DEBUG_LEVEL=0"
            )
            message(STATUS "  CXX bridge: Using MSVC release runtime (/MD) with iterator debug level 0")
        endif()
    endif()


    set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/../rust")

    # Set environment variables for Rust compilation
    if(APPLE)
        corrosion_set_env_vars(test_crate
            "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
            "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        corrosion_set_env_vars(test_crate "CARGO_TARGET_DIR=${RUST_TARGET_DIR}")
    endif()

    add_test(
        NAME rust_tests
        COMMAND ${RUST_CARGO} test --manifest-path "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    # Set environment for Rust tests
    if(APPLE)
        set_tests_properties(rust_tests PROPERTIES
            ENVIRONMENT "CARGO_TARGET_DIR=${RUST_TARGET_DIR};MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        )
    else()
        set_tests_properties(rust_tests PROPERTIES
            ENVIRONMENT "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
        )
    endif()

    message(STATUS "Rust workspace imported successfully")
    message(STATUS "  Rust target directory: ${RUST_TARGET_DIR}")
    message(STATUS "  Rust tests added to CTest")

endfunction()
