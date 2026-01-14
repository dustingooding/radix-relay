#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <filesystem>
#include <platform/env_utils.hpp>
#include <signal/signal_bridge.hpp>
#include <tuple>

TEST_CASE("Node Identity Functions", "[node_identity]")
{
  SECTION("get_node_fingerprint returns deterministic fingerprint")
  {
    {
      auto bridge = std::make_shared<radix_relay::signal::bridge>(
        std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_node_identity_fingerprint.db");

      // cppcheck-suppress duplicateAssignExpression
      auto fingerprint1 = bridge->get_node_fingerprint();
      auto fingerprint2 = bridge->get_node_fingerprint();

      CHECK(fingerprint1.starts_with("RDX:"));
      CHECK(fingerprint1.length() == 68);
      CHECK(fingerprint1 == fingerprint2);
    }

    std::ignore = std::filesystem::remove(
      std::filesystem::path(radix_relay::platform::get_temp_directory()) / "test_node_identity_fingerprint.db");
  }
}
