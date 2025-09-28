include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(radix_relay_setup_dependencies)

  if(NOT TARGET fmtlib::fmtlib)
    cpmaddpackage("gh:fmtlib/fmt#11.1.4")
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME
      spdlog
      VERSION
      1.15.2
      GITHUB_REPOSITORY
      "gabime/spdlog"
      OPTIONS
      "SPDLOG_FMT_EXTERNAL ON")
  endif()

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage("gh:catchorg/Catch2@3.8.1")
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage("gh:CLIUtils/CLI11@2.5.0")
  endif()

  if(NOT TARGET ftxui::screen)
    cpmaddpackage("gh:ArthurSonzogni/FTXUI@6.0.2")
  endif()

  if(NOT TARGET tools::tools)
    cpmaddpackage("gh:lefticus/tools#update_build_system")
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
      NAME
      corrosion
      VERSION
      0.5.2
      GITHUB_REPOSITORY
      "corrosion-rs/corrosion"
      OPTIONS
      "CORROSION_VERBOSE_OUTPUT ON"
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

  if(NOT TARGET Boost::asio)
    cpmaddpackage(
      NAME Boost
      GITHUB_REPOSITORY boostorg/boost
      VERSION 1.89.0
      GIT_TAG boost-1.89.0
      GIT_SHALLOW TRUE
      GIT_SUBMODULES ""
      SYSTEM YES
      OPTIONS
        "BOOST_INCLUDE_LIBRARIES asio;system;date_time;regex"
        "BOOST_ENABLE_CMAKE ON"
    )
  endif()

  if(NOT TARGET nlohmann_json::nlohmann_json)
    cpmaddpackage("gh:nlohmann/json@3.12.0")
  endif()

endfunction()
