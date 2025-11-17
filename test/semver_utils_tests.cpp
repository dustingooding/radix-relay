#include <catch2/catch_test_macros.hpp>
#include <core/semver_utils.hpp>
#include <nostr/semver_utils.hpp>

TEST_CASE("semver_utils checks version compatibility", "[semver][utils]")
{
  SECTION("versions equal to minimum are compatible")
  {
    REQUIRE(radix_relay::core::is_version_compatible("0.4.0", "0.4.0"));
    REQUIRE(radix_relay::core::is_version_compatible("1.0.0", "1.0.0"));
    REQUIRE(radix_relay::core::is_version_compatible("0.1.0", "0.1.0"));
  }

  SECTION("versions greater than minimum are compatible")
  {
    REQUIRE(radix_relay::core::is_version_compatible("0.5.0", "0.4.0"));
    REQUIRE(radix_relay::core::is_version_compatible("1.0.0", "0.4.0"));
    REQUIRE(radix_relay::core::is_version_compatible("0.4.1", "0.4.0"));
    REQUIRE(radix_relay::core::is_version_compatible("0.4.0-beta.2", "0.4.0-beta.1"));
  }

  SECTION("versions less than minimum are not compatible")
  {
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("0.3.0", "0.4.0"));
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("0.3.9", "0.4.0"));
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("0.1.0", "1.0.0"));
  }

  SECTION("invalid version strings return false")
  {
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("invalid", "0.4.0"));
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("0.4.0", "invalid"));
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("", "0.4.0"));
    REQUIRE_FALSE(radix_relay::core::is_version_compatible("0.4.0", ""));
  }

  SECTION("prerelease versions are handled correctly")
  {
    REQUIRE(radix_relay::core::is_version_compatible("0.4.0", "0.4.0-beta.1"));
    REQUIRE(radix_relay::core::is_version_compatible("0.4.0-rc.1", "0.4.0-beta.1"));
  }
}

TEST_CASE("semver_utils extracts version from tag", "[semver][utils]")
{
  SECTION("extracts version from radix_version tag")
  {
    const std::vector<std::vector<std::string>> tags = {
      { "d", "radix_prekey_bundle_v1" }, { "radix_version", "0.4.0" }, { "some_other_tag", "value" }
    };

    auto version = radix_relay::nostr::extract_version_from_tags(tags);
    REQUIRE(version.has_value());
    if (version.has_value()) { CHECK(version.value() == "0.4.0"); }
  }

  SECTION("returns nullopt when radix_version tag is missing")
  {
    const std::vector<std::vector<std::string>> tags = { { "d", "radix_prekey_bundle_v1" }, { "some_tag", "value" } };

    auto version = radix_relay::nostr::extract_version_from_tags(tags);
    REQUIRE_FALSE(version.has_value());
  }

  SECTION("returns nullopt for empty tags")
  {
    const std::vector<std::vector<std::string>> tags = {};

    auto version = radix_relay::nostr::extract_version_from_tags(tags);
    REQUIRE_FALSE(version.has_value());
  }

  SECTION("handles radix_version tag with empty value")
  {
    const std::vector<std::vector<std::string>> tags = { { "radix_version", "" } };

    auto version = radix_relay::nostr::extract_version_from_tags(tags);
    REQUIRE(version.has_value());
    if (version.has_value()) { CHECK(version.value().empty()); }
  }

  SECTION("handles radix_version tag with only tag name")
  {
    const std::vector<std::vector<std::string>> tags = { { "radix_version" } };

    auto version = radix_relay::nostr::extract_version_from_tags(tags);
    REQUIRE_FALSE(version.has_value());
  }
}
