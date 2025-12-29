# Rust Integration via Corrosion
# This file handles the setup and configuration of Rust compilation within CMake

include(cmake/RustQualityChecks.cmake)

function(setup_rust_workspace)
    # Generate Cargo.toml from template with version from VERSION file
    # configure_file automatically tracks changes to VERSION and regenerates when needed
    configure_file(
        "${CMAKE_SOURCE_DIR}/rust/Cargo.toml.in"
        "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        @ONLY
    )

    message(STATUS "Configured Rust workspace with version ${PROJECT_VERSION}")

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

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ENV{CARGO_BUILD_TYPE} "debug")
        message(STATUS "  Rust Debug build configured")
    else()
        set(ENV{CARGO_BUILD_TYPE} "release")
        message(STATUS "  Rust Release build configured")
    endif()

    set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/../rust")

    set(ENV{CARGO_TARGET_DIR} "${RUST_TARGET_DIR}")

    corrosion_import_crate(
        MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        CRATES signal_bridge
    )

    corrosion_add_cxxbridge(
        signal_bridge_cxx
        CRATE signal_bridge
        FILES lib.rs
    )

    # Mark cxxbridge headers as system to suppress warnings
    # Note: We keep INTERFACE_INCLUDE_DIRECTORIES and add INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
    # Both are needed - regular includes for targets to find headers, system to suppress warnings
    get_target_property(SIGNAL_BRIDGE_INCLUDES signal_bridge_cxx INTERFACE_INCLUDE_DIRECTORIES)
    if(SIGNAL_BRIDGE_INCLUDES)
        set_property(TARGET signal_bridge_cxx APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${SIGNAL_BRIDGE_INCLUDES}"
        )
    endif()

    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            corrosion_set_env_vars(signal_bridge
                "CFLAGS=/MDd /D_ITERATOR_DEBUG_LEVEL=2"
                "CXXFLAGS=/MDd /D_ITERATOR_DEBUG_LEVEL=2"
            )
            message(STATUS "  CXX bridge: Using MSVC debug runtime (/MDd) with iterator debug level 2")
        else()
            corrosion_set_env_vars(signal_bridge
                "CFLAGS=/MD /D_ITERATOR_DEBUG_LEVEL=0"
                "CXXFLAGS=/MD /D_ITERATOR_DEBUG_LEVEL=0"
            )
            message(STATUS "  CXX bridge: Using MSVC release runtime (/MD) with iterator debug level 0")
        endif()

        target_link_libraries(signal_bridge_cxx INTERFACE
            bcrypt
            OpenSSL::SSL
            OpenSSL::Crypto
        )
    endif()

    if(APPLE)
        corrosion_set_env_vars(signal_bridge
            "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
            "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
        # Link required system frameworks for Rust dependencies (chrono/iana_time_zone)
        target_link_libraries(signal_bridge_cxx INTERFACE "-framework CoreFoundation")
    else()
        corrosion_set_env_vars(signal_bridge "CARGO_TARGET_DIR=${RUST_TARGET_DIR}")
    endif()

    add_test(
        NAME rust_tests
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${RUST_TARGET_DIR} ${RUST_CARGO} test --manifest-path "${CMAKE_SOURCE_DIR}/rust/Cargo.toml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

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

    radix_relay_setup_rust_quality_checks("${CMAKE_SOURCE_DIR}/rust")

endfunction()
