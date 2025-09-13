#include <catch2/catch_test_macros.hpp>
#include <string>

#include "crypto_utils_cxx/lib.h"

TEST_CASE("Rust CXX Bridge Integration", "[rust][cxx]")
{

  SECTION("generate_node_fingerprint produces deterministic output")
  {
    auto fingerprint1 = radix_relay::generate_node_fingerprint("testhost", "testuser");
    auto fingerprint2 = radix_relay::generate_node_fingerprint("testhost", "testuser");
    auto fingerprint3 = radix_relay::generate_node_fingerprint("otherhost", "testuser");

    REQUIRE(std::string(fingerprint1).starts_with("RDX:"));
    REQUIRE(std::string(fingerprint1).length() == 20);
    REQUIRE(std::string(fingerprint1) == std::string(fingerprint2));
    REQUIRE(std::string(fingerprint1) != std::string(fingerprint3));
  }

  SECTION("get_node_identity_fingerprint returns valid format")
  {
    auto fingerprint = radix_relay::get_node_identity_fingerprint();

    REQUIRE(std::string(fingerprint).starts_with("RDX:"));
    REQUIRE(std::string(fingerprint).length() == 20);

    auto fingerprint2 = radix_relay::get_node_identity_fingerprint();
    REQUIRE(std::string(fingerprint) == std::string(fingerprint2));
  }
}
