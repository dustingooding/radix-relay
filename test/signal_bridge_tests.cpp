#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <platform/env_utils.hpp>
#include <signal/signal_bridge.hpp>

TEST_CASE("signal::bridge basic functionality", "[signal][wrapper]")
{
  auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                  / ("test_wrapper_" + std::to_string(timestamp) + ".db"))
                   .string();

  SECTION("Wrapper can be constructed from db path")
  {
    {
      auto wrapper = radix_relay::signal::bridge(db_path);
      auto fingerprint = wrapper.get_node_fingerprint();

      REQUIRE(fingerprint.starts_with("RDX:"));
      REQUIRE(fingerprint.length() == 68);
    }
    std::filesystem::remove(db_path);
  }

  SECTION("Wrapper can be constructed from shared_ptr with db path")
  {
    {
      auto wrapper = std::make_shared<radix_relay::signal::bridge>(db_path);
      auto fingerprint = wrapper->get_node_fingerprint();

      REQUIRE(fingerprint.starts_with("RDX:"));
      REQUIRE(fingerprint.length() == 68);
    }
    std::filesystem::remove(db_path);
  }
}

TEST_CASE("signal::bridge contact management", "[signal][wrapper][contacts]")
{
  auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_wrapper_alice_" + std::to_string(timestamp) + ".db"))
                    .string();
  auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                 / ("test_wrapper_bob_" + std::to_string(timestamp) + ".db"))
                  .string();

  SECTION("list_contacts returns empty for fresh identity")
  {
    {
      auto wrapper = radix_relay::signal::bridge(alice_db);
      auto contacts = wrapper.list_contacts();

      REQUIRE(contacts.empty());
    }
    std::filesystem::remove(alice_db);
  }

  SECTION("Extract RDX from bundle without establishing session")
  {
    {
      auto alice = std::make_shared<radix_relay::signal::bridge>(alice_db);
      auto bob = std::make_shared<radix_relay::signal::bridge>(bob_db);

      auto bob_bundle = bob->generate_prekey_bundle_announcement("test-0.1.0");
      auto bob_bundle_json = nlohmann::json::parse(bob_bundle);
      auto bob_bundle_base64 = bob_bundle_json["content"].template get<std::string>();

      auto extracted_rdx = alice->extract_rdx_from_bundle_base64(bob_bundle_base64);

      REQUIRE(extracted_rdx.starts_with("RDX:"));
      REQUIRE(extracted_rdx.length() == 68);

      auto contacts = alice->list_contacts();
      REQUIRE(contacts.empty());

      auto bob_rdx = alice->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

      REQUIRE(extracted_rdx == bob_rdx);
    }
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Session establishment and contact listing")
  {
    {
      auto alice = std::make_shared<radix_relay::signal::bridge>(alice_db);
      auto bob = std::make_shared<radix_relay::signal::bridge>(bob_db);

      auto bob_bundle = bob->generate_prekey_bundle_announcement("test-0.1.0");
      auto bob_bundle_json = nlohmann::json::parse(bob_bundle);
      auto bob_bundle_base64 = bob_bundle_json["content"].template get<std::string>();

      auto bob_rdx = alice->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

      auto contacts = alice->list_contacts();
      REQUIRE(contacts.size() == 1);
      REQUIRE(contacts[0].rdx_fingerprint == bob_rdx);
      REQUIRE(contacts[0].user_alias == "bob");
      REQUIRE(contacts[0].has_active_session);
    }
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}

TEST_CASE("signal::bridge encryption/decryption", "[signal][wrapper][encryption]")
{
  auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_wrapper_encrypt_alice_" + std::to_string(timestamp) + ".db"))
                    .string();
  auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                 / ("test_wrapper_encrypt_bob_" + std::to_string(timestamp) + ".db"))
                  .string();

  SECTION("Encrypt and decrypt message through wrapper")
  {
    {
      auto alice = std::make_shared<radix_relay::signal::bridge>(alice_db);
      auto bob = std::make_shared<radix_relay::signal::bridge>(bob_db);

      auto bob_bundle = bob->generate_prekey_bundle_announcement("test-0.1.0");
      auto bob_bundle_json = nlohmann::json::parse(bob_bundle);
      auto bob_bundle_base64 = bob_bundle_json["content"].template get<std::string>();
      auto bob_rdx = alice->add_contact_and_establish_session_from_base64(bob_bundle_base64, "");

      auto alice_bundle = alice->generate_prekey_bundle_announcement("test-0.1.0");
      auto alice_bundle_json = nlohmann::json::parse(alice_bundle);
      auto alice_bundle_base64 = alice_bundle_json["content"].template get<std::string>();
      auto alice_rdx = bob->add_contact_and_establish_session_from_base64(alice_bundle_base64, "");

      const std::string plaintext = "Hello Bob!";
      const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

      auto encrypted = alice->encrypt_message(bob_rdx, message_bytes);
      auto decrypted = bob->decrypt_message(alice_rdx, encrypted);

      std::string decrypted_str(decrypted.begin(), decrypted.end());
      REQUIRE(decrypted_str == plaintext);
    }
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}

TEST_CASE("signal::bridge alias management", "[signal][wrapper][alias]")
{
  auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto alice_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                   / ("test_wrapper_alias_alice_" + std::to_string(timestamp) + ".db"))
                    .string();
  auto bob_db = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                 / ("test_wrapper_alias_bob_" + std::to_string(timestamp) + ".db"))
                  .string();

  SECTION("Assign and lookup contact alias")
  {
    {
      auto alice = std::make_shared<radix_relay::signal::bridge>(alice_db);
      auto bob = std::make_shared<radix_relay::signal::bridge>(bob_db);

      auto bob_bundle = bob->generate_prekey_bundle_announcement("test-0.1.0");
      auto bob_bundle_json = nlohmann::json::parse(bob_bundle);
      auto bob_bundle_base64 = bob_bundle_json["content"].template get<std::string>();
      auto bob_rdx = alice->add_contact_and_establish_session_from_base64(bob_bundle_base64, "");

      const std::string alias = "BobTheBuilder";
      alice->assign_contact_alias(bob_rdx, alias);

      auto contact = alice->lookup_contact(alias);
      REQUIRE(contact.rdx_fingerprint == bob_rdx);
      REQUIRE(contact.user_alias == alias);
    }
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }
}

TEST_CASE("signal::bridge generate_empty_bundle_announcement creates valid empty event", "[signal][wrapper][bundle]")
{
  auto timestamp =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  auto db_path = (std::filesystem::path(radix_relay::platform::get_temp_directory())
                  / ("test_empty_bundle_" + std::to_string(timestamp) + ".db"))
                   .string();

  {
    auto wrapper = radix_relay::signal::bridge(db_path);
    auto announcement_json = wrapper.generate_empty_bundle_announcement("0.4.0");
    auto event = nlohmann::json::parse(announcement_json);

    REQUIRE(event["kind"].template get<int>() == 30078);
    REQUIRE(event["content"].template get<std::string>().empty());

    const auto tags = event["tags"];
    bool found_d_tag = false;
    bool found_version_tag = false;
    bool found_rdx_tag = false;

    for (const auto &tag : tags) {
      if (tag[0] == "d") {
        found_d_tag = true;
        REQUIRE(tag[1] == "radix_prekey_bundle_v1");
      } else if (tag[0] == "radix_version") {
        found_version_tag = true;
        REQUIRE(tag[1] == "0.4.0");
      } else if (tag[0] == "rdx") {
        found_rdx_tag = true;
      }
    }

    REQUIRE(found_d_tag);
    REQUIRE(found_version_tag);
    REQUIRE_FALSE(found_rdx_tag);
  }
  std::filesystem::remove(db_path);
}
