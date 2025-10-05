#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "signal_bridge_cxx/lib.h"
#include <radix_relay/platform/env_utils.hpp>

TEST_CASE("SignalBridge Node Fingerprint Integration", "[signal][fingerprint][cxx]")
{
  SECTION("generate_node_fingerprint produces deterministic output using SignalBridge")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                    / ("test_signal_fingerprint_" + std::to_string(timestamp) + ".db"))
                     .string();

    {
      auto bridge = radix_relay::new_signal_bridge(db_path.c_str());

      auto identity = radix_relay::NodeIdentity{
        .hostname = "testhost", .username = "testuser", .platform = "linux", .mac_address = "", .install_id = ""
      };

      auto fingerprint1 = radix_relay::generate_node_fingerprint(*bridge, identity);
      auto fingerprint2 = radix_relay::generate_node_fingerprint(*bridge, identity);
      auto fingerprint3 = radix_relay::generate_node_fingerprint(*bridge, identity);

      REQUIRE(std::string(fingerprint1).starts_with("RDX:"));
      REQUIRE(std::string(fingerprint1).length() == 68);
      REQUIRE(std::string(fingerprint1) == std::string(fingerprint2));
      REQUIRE(std::string(fingerprint2) == std::string(fingerprint3));
    }

    std::filesystem::remove(db_path);
  }
}


TEST_CASE("SignalBridge Error Message Propagation", "[signal][error][cxx]")
{
  SECTION("Empty peer name produces specific error message")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                    / ("test_empty_peer_name_" + std::to_string(timestamp) + ".db"))
                     .string();

    {
      auto bridge = radix_relay::new_signal_bridge(db_path.c_str());

      REQUIRE_THROWS_WITH(
        radix_relay::clear_peer_session(*bridge, ""), Catch::Matchers::ContainsSubstring("Specify a peer name"));
    }

    std::filesystem::remove(db_path);
  }
}

TEST_CASE("SignalBridge CXX Integration", "[signal][cxx]")
{
  SECTION("SignalBridge constructor creates valid instance")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                    / ("test_signal_bridge_constructor_" + std::to_string(timestamp) + ".db"))
                     .string();

    {
      REQUIRE_NOTHROW([&]() { auto bridge = radix_relay::new_signal_bridge(db_path.c_str()); }());
    }

    std::filesystem::remove(db_path);
  }

  SECTION("Alice and Bob can exchange encrypted messages")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_signal_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_signal_bob_" + std::to_string(timestamp) + ".db"))
                    .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());

      auto bob_bundle = radix_relay::generate_pre_key_bundle(*bob);
      REQUIRE(!bob_bundle.empty());

      REQUIRE_NOTHROW(
        [&]() { radix_relay::establish_session(*alice, "bob", rust::Slice<const uint8_t>{ bob_bundle }); }());

      std::string plaintext = "Hello Bob! This is Alice using SignalBridge from C++.";
      std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());

      auto ciphertext = radix_relay::encrypt_message(*alice, "bob", rust::Slice<const uint8_t>{ plaintext_bytes });
      REQUIRE(!ciphertext.empty());
      REQUIRE(ciphertext.size() > plaintext_bytes.size());

      auto decrypted = radix_relay::decrypt_message(*bob, "alice", rust::Slice<const uint8_t>{ ciphertext });
      REQUIRE(!decrypted.empty());
      REQUIRE(decrypted.size() == plaintext_bytes.size());

      std::string decrypted_string(decrypted.begin(), decrypted.end());
      REQUIRE(decrypted_string == plaintext);
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Session management functions work correctly")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_signal_session_mgmt_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_signal_session_mgmt_bob_" + std::to_string(timestamp) + ".db"))
                    .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());

      auto bob_bundle = radix_relay::generate_pre_key_bundle(*bob);
      REQUIRE_NOTHROW(
        [&]() { radix_relay::establish_session(*alice, "bob", rust::Slice<const uint8_t>{ bob_bundle }); }());

      REQUIRE_NOTHROW([&]() { radix_relay::clear_peer_session(*alice, "bob"); }());

      REQUIRE_NOTHROW([&]() { radix_relay::clear_all_sessions(*alice); }());

      REQUIRE_NOTHROW([&]() { radix_relay::reset_identity(*alice); }());
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}
