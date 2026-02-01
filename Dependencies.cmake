include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(radix_relay_setup_dependencies)

  if(NOT TARGET fmt::fmt)
    cpmaddpackage(
      NAME fmt
      GITHUB_REPOSITORY fmtlib/fmt
      GIT_TAG 11.1.4
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(fmt_ADDED)
      message(STATUS "System fmt not found, built from source via CPM")
    else()
      message(STATUS "Found system fmt ${fmt_VERSION}")
    endif()
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME spdlog
      VERSION 1.15.2
      GITHUB_REPOSITORY "gabime/spdlog"
      OPTIONS "SPDLOG_FMT_EXTERNAL ON"
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(spdlog_ADDED)
      message(STATUS "System spdlog not found, built from source via CPM")
    else()
      message(STATUS "Found system spdlog ${spdlog_VERSION}")
    endif()
  endif()

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage(
      NAME Catch2
      GITHUB_REPOSITORY catchorg/Catch2
      VERSION 3.8.1
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(Catch2_ADDED)
      message(STATUS "System Catch2 not found, built from source via CPM")
    else()
      message(STATUS "Found system Catch2 ${Catch2_VERSION}")
    endif()
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage(
      NAME CLI11
      GITHUB_REPOSITORY CLIUtils/CLI11
      VERSION 2.5.0
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(CLI11_ADDED)
      message(STATUS "System CLI11 not found, built from source via CPM")
    else()
      message(STATUS "Found system CLI11 ${CLI11_VERSION}")
    endif()
  endif()

  if(NOT TARGET replxx::replxx)
    cpmaddpackage(
      NAME replxx
      GITHUB_REPOSITORY AmokHuginnsson/replxx
      GIT_TAG release-0.0.4
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(replxx_ADDED)
      message(STATUS "System replxx not found, built from source via CPM")
    else()
      message(STATUS "Found system replxx")
    endif()
  endif()

  if(NOT TARGET tools::tools)
    cpmaddpackage(
      NAME tools
      GITHUB_REPOSITORY lefticus/tools
      GIT_TAG update_build_system
      SYSTEM YES
    )
  endif()

  if(NOT TARGET corrosion)
    if(WIN32)
      set(Rust_TOOLCHAIN "stable-x86_64-pc-windows-msvc" CACHE STRING "Rust toolchain to use")
    elseif(APPLE)
      set(Rust_TOOLCHAIN "stable-aarch64-apple-darwin" CACHE STRING "Rust toolchain to use")
    else()
      set(Rust_TOOLCHAIN "stable-x86_64-unknown-linux-gnu" CACHE STRING "Rust toolchain to use")
    endif()

    cpmaddpackage(
      NAME corrosion
      VERSION 0.5.2
      GITHUB_REPOSITORY "corrosion-rs/corrosion"
      OPTIONS "CORROSION_VERBOSE_OUTPUT ON"
      SYSTEM YES
    )
  endif()

  find_package(Protobuf REQUIRED)
  if(Protobuf_FOUND)
    message(STATUS "Found Protobuf: ${Protobuf_VERSION}")
    message(STATUS "  Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")
    message(STATUS "  Protobuf libraries: ${Protobuf_LIBRARIES}")
    message(STATUS "  Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
  endif()

  find_package(OpenSSL REQUIRED)
  if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
    message(STATUS "  OpenSSL libraries: ${OPENSSL_LIBRARIES}")
  endif()

  # Try to find system Boost
  # Note: Arch Linux Boost packages provide CONFIG files but may not create all targets
  find_package(Boost 1.89.0 QUIET CONFIG)

  if(Boost_FOUND)
    message(STATUS "Found system Boost ${Boost_VERSION} at ${Boost_INCLUDE_DIRS}")

    # Create Boost::system target if it doesn't exist
    # Note: Boost::system became header-only in Boost 1.69+, so no library file exists
    if(NOT TARGET Boost::system)
      add_library(Boost::system INTERFACE IMPORTED)
      target_include_directories(Boost::system INTERFACE ${Boost_INCLUDE_DIRS})
      message(STATUS "Created Boost::system header-only target")
    else()
      message(STATUS "Found Boost::system target")
    endif()

    # Header-only libraries don't create targets automatically, create them
    if(NOT TARGET Boost::asio)
      add_library(Boost::asio INTERFACE IMPORTED)
      target_include_directories(Boost::asio INTERFACE ${Boost_INCLUDE_DIRS})
    endif()

    if(NOT TARGET Boost::beast)
      add_library(Boost::beast INTERFACE IMPORTED)
      target_include_directories(Boost::beast INTERFACE ${Boost_INCLUDE_DIRS})
    endif()

    if(NOT TARGET Boost::uuid)
      add_library(Boost::uuid INTERFACE IMPORTED)
      target_include_directories(Boost::uuid INTERFACE ${Boost_INCLUDE_DIRS})
    endif()
  endif()

  if(NOT Boost_FOUND)
    message(STATUS "System Boost not found, building from source via CPM...")
    if(NOT TARGET Boost::asio)
      cpmaddpackage(
        NAME Boost
        GITHUB_REPOSITORY boostorg/boost
        VERSION 1.89.0
        GIT_TAG boost-1.89.0
        GIT_SHALLOW TRUE
        OPTIONS
          "BOOST_INCLUDE_LIBRARIES asio;beast;system;date_time;regex;logic;uuid"
          "BOOST_ENABLE_CMAKE ON"
        SYSTEM YES
      )
    endif()

    # Beast is header-only but doesn't create a CMake target with BOOST_INCLUDE_LIBRARIES
    # Create our own interface library to provide the Beast headers
    if(NOT TARGET Boost::beast)
      add_library(Boost::beast INTERFACE IMPORTED GLOBAL)
      target_include_directories(Boost::beast SYSTEM INTERFACE
        "${Boost_SOURCE_DIR}/libs/beast/include"
        "${Boost_SOURCE_DIR}/libs/endian/include"
        "${Boost_SOURCE_DIR}/libs/static_string/include"
        "${Boost_SOURCE_DIR}/libs/logic/include"
      )
      target_link_libraries(Boost::beast INTERFACE Boost::asio Boost::system)
    endif()

    # UUID is header-only but doesn't create a CMake target with BOOST_INCLUDE_LIBRARIES
    # Create our own interface library to provide the UUID headers
    if(NOT TARGET Boost::uuid)
      add_library(Boost::uuid INTERFACE IMPORTED GLOBAL)
      target_include_directories(Boost::uuid SYSTEM INTERFACE
        "${Boost_SOURCE_DIR}/libs/uuid/include"
        "${Boost_SOURCE_DIR}/libs/random/include"
        "${Boost_SOURCE_DIR}/libs/io/include"
        "${Boost_SOURCE_DIR}/libs/serialization/include"
      )
    endif()
  else()
    message(STATUS "Using system-provided Boost ${Boost_VERSION}")
    # System Boost should already have Boost::beast and Boost::uuid targets
  endif()

  if(NOT TARGET nlohmann_json::nlohmann_json)
    cpmaddpackage(
      NAME nlohmann_json
      GITHUB_REPOSITORY nlohmann/json
      VERSION 3.12.0
      SYSTEM YES
      FIND_PACKAGE_ARGUMENTS "QUIET CONFIG"
    )
    if(nlohmann_json_ADDED)
      message(STATUS "System nlohmann_json not found, built from source via CPM")
    else()
      message(STATUS "Found system nlohmann_json ${nlohmann_json_VERSION}")
    endif()
  endif()

  if(NOT TARGET semver::semver)
    cpmaddpackage(
      NAME semver
      GITHUB_REPOSITORY z4kn4fein/cpp-semver
      GIT_TAG v0.3.3
      SYSTEM YES
    )
  endif()

  if(NOT TARGET Slint::Slint)
    cpmaddpackage(
      NAME Slint
      GITHUB_REPOSITORY slint-ui/slint
      GIT_TAG v1.14.1
      OPTIONS
        "SLINT_BUILD_COMPILER ON"
        "SLINT_BUILD_RUNTIME ON"
        "SLINT_FEATURE_BACKEND_WINIT ON"
        "SLINT_FEATURE_RENDERER_SOFTWARE OFF"
        "SLINT_FEATURE_INTERPRETER OFF"
        "SLINT_BUILD_EXAMPLES OFF"
        "SLINT_BUILD_TESTING OFF"
        "SLINT_FEATURE_BACKEND_QT OFF"
        "SLINT_FEATURE_RENDERER_FEMTOVG ON"
        "SLINT_FEATURE_ACCESSIBILITY OFF"
        "SLINT_FEATURE_EXPERIMENTAL ON"
        "SLINT_FEATURE_TESTING ON"
      SYSTEM YES
    )
    if(Slint_ADDED)
      message(STATUS "System Slint not found, built from source via CPM")
    else()
      message(STATUS "Found system Slint ${Slint_VERSION}")
    endif()
  endif()

  if(NOT TARGET simpleble::simpleble)
    cpmaddpackage(
      NAME SimpleBLE
      GITHUB_REPOSITORY simpleble/simpleble
      GIT_TAG v0.6.1
      SOURCE_SUBDIR simpleble
      OPTIONS
        "SIMPLEBLE_PLAIN OFF"
        "SIMPLEBLE_USE_SESSION_DBUS OFF"
        "SIMPLEBLE_LOG_LEVEL INFO"
        "SIMPLEBLUEZ_LOG_LEVEL FATAL"
        "SIMPLEDBUS_LOG_LEVEL FATAL"
      SYSTEM YES
    )
    if(SimpleBLE_ADDED)
      message(STATUS "System SimpleBLE not found, built from source via CPM")
      if(TARGET simpleble AND NOT TARGET simpleble::simpleble)
        add_library(simpleble::simpleble ALIAS simpleble)
        message(STATUS "Created simpleble::simpleble alias for simpleble target")
      endif()
      # Suppress specific deprecation warnings on MSVC for SimpleBLE
      if(TARGET simpleble)
        target_compile_options(simpleble PRIVATE
          $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        )
        target_compile_definitions(simpleble PRIVATE
          $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        )
      endif()
      if(TARGET simpleble-c)
        target_compile_options(simpleble-c PRIVATE
          $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
        )
        target_compile_definitions(simpleble-c PRIVATE
          $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        )
      endif()
    else()
      message(STATUS "Found system SimpleBLE ${SimpleBLE_VERSION}")
    endif()
  endif()

endfunction()
