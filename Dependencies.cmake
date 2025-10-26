include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(radix_relay_setup_dependencies)

  if(NOT TARGET fmtlib::fmtlib)
    cpmaddpackage(
      NAME fmt
      GITHUB_REPOSITORY fmtlib/fmt
      GIT_TAG 11.1.4
      SYSTEM YES
    )
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME spdlog
      VERSION 1.15.2
      GITHUB_REPOSITORY "gabime/spdlog"
      OPTIONS "SPDLOG_FMT_EXTERNAL ON"
      SYSTEM YES
    )
  endif()

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage(
      NAME Catch2
      GITHUB_REPOSITORY catchorg/Catch2
      VERSION 3.8.1
      SYSTEM YES
    )
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage(
      NAME CLI11
      GITHUB_REPOSITORY CLIUtils/CLI11
      VERSION 2.5.0
      SYSTEM YES
    )
  endif()

  if(NOT TARGET ftxui::screen)
    cpmaddpackage(
      NAME FTXUI
      GITHUB_REPOSITORY ArthurSonzogni/FTXUI
      VERSION 6.0.2
      SYSTEM YES
    )
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

  find_package(Boost 1.89.0 QUIET COMPONENTS asio beast date_time logic regex system uuid)

  if(NOT Boost_FOUND)
    message(STATUS "System Boost not found, building from source via CPM...")
    if(NOT TARGET Boost::asio)
      cpmaddpackage(s
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
    )
  endif()

endfunction()
