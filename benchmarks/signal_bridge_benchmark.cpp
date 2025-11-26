#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <signal/signal_bridge.hpp>

namespace radix_relay::signal::test {

TEST_CASE("Signal Bridge Performance Benchmarks", "[benchmark][signal]")
{
  SECTION("Key generation operations")
  {
    const auto db_path = (std::filesystem::temp_directory_path() / "bench_keygen.db").string();
    std::filesystem::remove(db_path);
    auto bridge = std::make_shared<radix_relay::signal::bridge>(db_path);

    BENCHMARK("Generate prekey bundle announcement")
    {
      return bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    };

    bridge.reset();
    std::filesystem::remove(db_path);
  }

  SECTION("Session establishment")
  {
    const auto alice_db = (std::filesystem::temp_directory_path() / "bench_session_alice.db").string();
    const auto bob_db = (std::filesystem::temp_directory_path() / "bench_session_bob.db").string();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);

    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db);

    const auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    const auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_info.announcement_json);
    const std::string bob_bundle_base64 = bob_bundle_parsed["content"].template get<std::string>();

    BENCHMARK_ADVANCED("Establish session from bundle (X3DH)")(Catch::Benchmark::Chronometer meter)
    {
      const auto fresh_alice_db = (std::filesystem::temp_directory_path() / "bench_session_alice_fresh.db").string();
      std::filesystem::remove(fresh_alice_db);

      auto fresh_alice_bridge = std::make_shared<radix_relay::signal::bridge>(fresh_alice_db);

      meter.measure([&]() -> std::string {
        return fresh_alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");
      });

      fresh_alice_bridge.reset();
      std::filesystem::remove(fresh_alice_db);
    };

    alice_bridge.reset();
    bob_bridge.reset();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Message encryption")
  {
    const auto alice_db = (std::filesystem::temp_directory_path() / "bench_encrypt_alice.db").string();
    const auto bob_db = (std::filesystem::temp_directory_path() / "bench_encrypt_bob.db").string();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);

    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db);

    const auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    const auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_info.announcement_json);
    const std::string bob_bundle_base64 = bob_bundle_parsed["content"].template get<std::string>();

    const auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");

    const std::string plaintext = "Benchmark message for encryption/decryption testing";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

    BENCHMARK("Encrypt message") { return alice_bridge->encrypt_message(bob_rdx, message_bytes); };

    alice_bridge.reset();
    bob_bridge.reset();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Message decryption with metadata")
  {
    const auto alice_db = (std::filesystem::temp_directory_path() / "bench_decrypt_meta_alice.db").string();
    const auto bob_db = (std::filesystem::temp_directory_path() / "bench_decrypt_meta_bob.db").string();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);

    auto alice_bridge = std::make_shared<radix_relay::signal::bridge>(alice_db);
    auto bob_bridge = std::make_shared<radix_relay::signal::bridge>(bob_db);

    const auto alice_bundle_info = alice_bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    const auto alice_bundle_parsed = nlohmann::json::parse(alice_bundle_info.announcement_json);
    const std::string alice_bundle_base64 = alice_bundle_parsed["content"].template get<std::string>();

    const auto bob_bundle_info = bob_bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    const auto bob_bundle_parsed = nlohmann::json::parse(bob_bundle_info.announcement_json);
    const std::string bob_bundle_base64 = bob_bundle_parsed["content"].template get<std::string>();

    const auto bob_rdx = alice_bridge->add_contact_and_establish_session_from_base64(bob_bundle_base64, "bob");
    const auto alice_rdx = bob_bridge->add_contact_and_establish_session_from_base64(alice_bundle_base64, "alice");

    const std::string plaintext = "Benchmark message for encryption/decryption testing";
    const std::vector<uint8_t> message_bytes(plaintext.begin(), plaintext.end());

    BENCHMARK_ADVANCED("Decrypt message with metadata")(Catch::Benchmark::Chronometer meter)
    {
      std::vector<std::vector<uint8_t>> encrypted_messages;
      encrypted_messages.reserve(static_cast<std::size_t>(meter.runs()));
      for (std::size_t i = 0; std::cmp_less(i, meter.runs()); ++i) {
        encrypted_messages.push_back(alice_bridge->encrypt_message(bob_rdx, message_bytes));
      }

      meter.measure([&](std::size_t idx) -> decryption_result {
        return bob_bridge->decrypt_message(alice_rdx, encrypted_messages[idx]);
      });
    };

    alice_bridge.reset();
    bob_bridge.reset();
    std::filesystem::remove(alice_db);
    std::filesystem::remove(bob_db);
  }

  SECTION("Bundle operations")
  {
    const auto db_path = (std::filesystem::temp_directory_path() / "bench_bundle.db").string();
    std::filesystem::remove(db_path);
    auto bridge = std::make_shared<radix_relay::signal::bridge>(db_path);

    const auto bundle_info = bridge->generate_prekey_bundle_announcement("bench-0.1.0");
    const auto bundle_parsed = nlohmann::json::parse(bundle_info.announcement_json);
    const std::string bundle_base64 = bundle_parsed["content"].template get<std::string>();

    BENCHMARK("Extract RDX from bundle") { return bridge->extract_rdx_from_bundle_base64(bundle_base64); };

    bridge.reset();
    std::filesystem::remove(db_path);
  }
}

}// namespace radix_relay::signal::test
