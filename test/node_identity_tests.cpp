#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <platform/env_utils.hpp>
#include <signal/node_identity.hpp>
#include <signal/signal_bridge.hpp>
#include <tuple>

TEST_CASE("Node Identity Functions", "[node_identity]")
{
  SECTION("create_node_identity returns consistent NodeIdentity")
  {
    auto identity1 = radix_relay::signal::create_node_identity();
    auto identity2 = radix_relay::signal::create_node_identity();

    REQUIRE(identity1.hostname == identity2.hostname);
    REQUIRE(identity1.username == identity2.username);
    REQUIRE(identity1.platform == identity2.platform);
  }

  SECTION("get_node_fingerprint returns deterministic fingerprint")
  {
    {
      auto bridge = std::make_shared<radix_relay::signal::bridge>(
        std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_node_identity_fingerprint.db");

      // cppcheck-suppress duplicateAssignExpression
      auto fingerprint1 = bridge->get_node_fingerprint();
      auto fingerprint2 = bridge->get_node_fingerprint();

      REQUIRE(fingerprint1.starts_with("RDX:"));
      REQUIRE(fingerprint1.length() == 68);
      REQUIRE(fingerprint1 == fingerprint2);
    }

    std::ignore = std::filesystem::remove(
      std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_node_identity_fingerprint.db");
  }
}
