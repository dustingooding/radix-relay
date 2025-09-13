#include <catch2/catch_test_macros.hpp>
#include <radix_relay/node_identity.hpp>

TEST_CASE("Node Identity Functions", "[node_identity]")
{
  SECTION("create_node_identity returns consistent NodeIdentity")
  {
    auto identity1 = radix_relay::create_node_identity();
    auto identity2 = radix_relay::create_node_identity();

    REQUIRE(identity1.hostname == identity2.hostname);
    REQUIRE(identity1.username == identity2.username);
    REQUIRE(identity1.platform == identity2.platform);
  }

  SECTION("get_node_fingerprint returns deterministic fingerprint")
  {
    auto fingerprint1 = radix_relay::get_node_fingerprint();
    auto fingerprint2 = radix_relay::get_node_fingerprint();

    REQUIRE(fingerprint1.starts_with("RDX:"));
    REQUIRE(fingerprint1.length() == 68);
    REQUIRE(fingerprint1 == fingerprint2);
  }
}
