#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "signal_bridge_cxx/lib.h"
#include <platform/env_utils.hpp>

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

      auto fingerprint1 = radix_relay::generate_node_fingerprint(*bridge);
      auto fingerprint2 = radix_relay::generate_node_fingerprint(*bridge);
      auto fingerprint3 = radix_relay::generate_node_fingerprint(*bridge);

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
      REQUIRE_NOTHROW([&]() -> void { auto bridge = radix_relay::new_signal_bridge(db_path.c_str()); }());
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

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto alice_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*alice);
      REQUIRE(not bob_bundle_with_metadata.bundle_bytes.empty());
      REQUIRE(not alice_bundle_with_metadata.bundle_bytes.empty());

      auto bob_rdx = radix_relay::add_contact_and_establish_session(
        *alice, rust::Slice<const uint8_t>{ bob_bundle_with_metadata.bundle_bytes }, "bob");
      auto alice_rdx = radix_relay::add_contact_and_establish_session(
        *bob, rust::Slice<const uint8_t>{ alice_bundle_with_metadata.bundle_bytes }, "alice");
      REQUIRE(std::string(bob_rdx).starts_with("RDX:"));
      REQUIRE(std::string(alice_rdx).starts_with("RDX:"));

      std::string plaintext = "Hello Bob! This is Alice using SignalBridge from C++.";
      std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());

      auto ciphertext =
        radix_relay::encrypt_message(*alice, bob_rdx.c_str(), rust::Slice<const uint8_t>{ plaintext_bytes });
      REQUIRE(not ciphertext.empty());
      REQUIRE(ciphertext.size() > plaintext_bytes.size());

      auto result = radix_relay::decrypt_message(*bob, alice_rdx.c_str(), rust::Slice<const uint8_t>{ ciphertext });
      REQUIRE(not result.plaintext.empty());
      REQUIRE(result.plaintext.size() == plaintext_bytes.size());

      std::string decrypted_string(result.plaintext.begin(), result.plaintext.end());
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

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto bob_bundle = bob_bundle_with_metadata.bundle_bytes;
      auto bob_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ bob_bundle }, "bob");
      REQUIRE(std::string(bob_rdx).starts_with("RDX:"));

      REQUIRE_NOTHROW([&]() -> void { radix_relay::clear_peer_session(*alice, bob_rdx.c_str()); }());

      REQUIRE_NOTHROW([&]() -> void { radix_relay::clear_all_sessions(*alice); }());

      REQUIRE_NOTHROW([&]() -> void { radix_relay::reset_identity(*alice); }());
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}

TEST_CASE("SignalBridge Contact Management", "[signal][contacts][cxx]")
{
  SECTION("Can add contact from bundle and lookup by RDX")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_contact_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_contact_bob_" + std::to_string(timestamp) + ".db"))
                    .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto bob_bundle = bob_bundle_with_metadata.bundle_bytes;
      REQUIRE(not bob_bundle.empty());

      auto bob_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ bob_bundle }, "");
      REQUIRE(not bob_rdx.empty());
      REQUIRE(std::string(bob_rdx).starts_with("RDX:"));

      auto contact = radix_relay::lookup_contact(*alice, bob_rdx.c_str());
      REQUIRE(std::string(contact.rdx_fingerprint) == std::string(bob_rdx));
      REQUIRE(contact.user_alias.empty());
      REQUIRE(contact.has_active_session);
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Can assign alias and lookup by alias")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_alias_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_alias_bob_" + std::to_string(timestamp) + ".db"))
                    .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto bob_bundle = bob_bundle_with_metadata.bundle_bytes;
      auto bob_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ bob_bundle }, "");

      REQUIRE_NOTHROW([&]() -> void { radix_relay::assign_contact_alias(*alice, bob_rdx.c_str(), "bob"); }());

      auto contact_by_alias = radix_relay::lookup_contact(*alice, "bob");
      REQUIRE(std::string(contact_by_alias.rdx_fingerprint) == std::string(bob_rdx));
      REQUIRE(std::string(contact_by_alias.user_alias) == "bob");
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Can list all contacts")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_list_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_list_bob_" + std::to_string(timestamp) + ".db"))
                    .string();
    auto charlie_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                       / ("test_list_charlie_" + std::to_string(timestamp) + ".db"))
                        .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());
      auto charlie = radix_relay::new_signal_bridge(charlie_db.c_str());

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto bob_bundle = bob_bundle_with_metadata.bundle_bytes;
      auto charlie_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*charlie);
      auto charlie_bundle = charlie_bundle_with_metadata.bundle_bytes;

      auto bob_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ bob_bundle }, "bob");
      auto charlie_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ charlie_bundle }, "");

      auto contacts = radix_relay::list_contacts(*alice);
      REQUIRE(contacts.size() == 2);

      bool found_bob = false;
      bool found_charlie = false;

      for (const auto &contact : contacts) {
        if (std::string(contact.rdx_fingerprint) == std::string(bob_rdx)) {
          found_bob = true;
          REQUIRE(std::string(contact.user_alias) == "bob");
        }
        if (std::string(contact.rdx_fingerprint) == std::string(charlie_rdx)) {
          found_charlie = true;
          REQUIRE(contact.user_alias.empty());
        }
      }

      REQUIRE(found_bob);
      REQUIRE(found_charlie);
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
    std::filesystem::remove(charlie_db);
  }
}

TEST_CASE("SignalBridge Bundle Announcement", "[signal][bundle][nostr][cxx]")
{
  SECTION("Can generate prekey bundle announcement with RDX tag")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_bundle_announcement_" + std::to_string(timestamp) + ".db"))
                      .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());

      auto bundle_info = radix_relay::generate_prekey_bundle_announcement(*alice, "1.0.0-test");
      REQUIRE(not bundle_info.announcement_json.empty());

      // Parse JSON to verify structure
      auto event_str = std::string(bundle_info.announcement_json);
      REQUIRE(event_str.find("\"kind\":30078") != std::string::npos);
      REQUIRE(event_str.find("\"rdx\"") != std::string::npos);
      REQUIRE(event_str.find("RDX:") != std::string::npos);
      REQUIRE(event_str.find("radix_version") != std::string::npos);
      REQUIRE(event_str.find("1.0.0-test") != std::string::npos);
      REQUIRE(event_str.find("radix_prekey_bundle_v1") != std::string::npos);
      REQUIRE(event_str.find("\"content\":") != std::string::npos);
    }

    std::filesystem::remove(alice_db);
  }

  SECTION("Can add contact and establish session in one call")
  {
    auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                     / ("test_contact_session_alice_" + std::to_string(timestamp) + ".db"))
                      .string();
    auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_contact_session_bob_" + std::to_string(timestamp) + ".db"))
                    .string();

    {
      auto alice = radix_relay::new_signal_bridge(alice_db.c_str());
      auto bob = radix_relay::new_signal_bridge(bob_db.c_str());

      auto bob_bundle_with_metadata = radix_relay::generate_pre_key_bundle(*bob);
      auto bob_bundle = bob_bundle_with_metadata.bundle_bytes;

      // Alice adds Bob as contact AND establishes session in one call
      auto bob_rdx =
        radix_relay::add_contact_and_establish_session(*alice, rust::Slice<const uint8_t>{ bob_bundle }, "bob");
      REQUIRE(std::string(bob_rdx).starts_with("RDX:"));

      // Verify contact and session exist
      auto contact = radix_relay::lookup_contact(*alice, bob_rdx.c_str());
      REQUIRE(std::string(contact.user_alias) == "bob");
      REQUIRE(contact.has_active_session);

      // Verify Alice can encrypt to Bob
      std::string plaintext = "Hello Bob!";
      std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());
      auto ciphertext =
        radix_relay::encrypt_message(*alice, bob_rdx.c_str(), rust::Slice<const uint8_t>{ plaintext_bytes });
      REQUIRE(not ciphertext.empty());
    }

    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}
