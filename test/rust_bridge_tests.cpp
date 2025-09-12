#include <catch2/catch_test_macros.hpp>
#include <string>

#include "test_crate_cxx/lib.h"

TEST_CASE("Rust CXX Bridge Integration", "[rust][cxx]")
{

  SECTION("hello_world function returns expected string")
  {
    auto result = hello_world();
    std::string expected = "Hello World from Rust via CXX bridge!";
    REQUIRE(std::string(result) == expected);
  }

  SECTION("add_numbers function performs correct arithmetic")
  {
    REQUIRE(add_numbers(2, 3) == 5);
    REQUIRE(add_numbers(-1, 1) == 0);
    REQUIRE(add_numbers(0, 0) == 0);
    REQUIRE(add_numbers(-5, -3) == -8);
    REQUIRE(add_numbers(100, 200) == 300);
  }

  SECTION("rust::String converts to std::string correctly")
  {
    auto rust_string = hello_world();
    std::string std_string = std::string(rust_string);

    REQUIRE(!std_string.empty());
    REQUIRE(std_string == "Hello World from Rust via CXX bridge!");
  }
}
