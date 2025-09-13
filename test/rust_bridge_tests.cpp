#include <catch2/catch_test_macros.hpp>
#include <string>

#include "crypto_utils_cxx/lib.h"

TEST_CASE("Rust CXX Bridge Integration", "[rust][cxx]")
{

  SECTION("generate_node_fingerprint produces deterministic output")
  {
    auto identity = radix_relay::NodeIdentity{ .hostname = "testhost",
      .username = "testuser",
      .platform = "linux",
      .machine_id = "",
      .mac_address = "",
      .install_id = "" };

    auto fingerprint1 = radix_relay::generate_node_fingerprint(identity);
    auto fingerprint2 = radix_relay::generate_node_fingerprint(identity);
    auto fingerprint3 = radix_relay::generate_node_fingerprint(identity);

    REQUIRE(std::string(fingerprint1).starts_with("RDX:"));
    REQUIRE(std::string(fingerprint1).length() == 68);
    REQUIRE(std::string(fingerprint1) == std::string(fingerprint2));
    REQUIRE(std::string(fingerprint2) == std::string(fingerprint3));
  }
}
