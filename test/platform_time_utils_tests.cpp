#include <catch2/catch_test_macros.hpp>
#include <platform/time_utils.hpp>
#include <regex>

TEST_CASE("format_current_time_hms returns HH:MM:SS format", "[platform][time]")
{
  auto result = radix_relay::platform::format_current_time_hms();

  const std::regex hms_pattern(R"(\d{2}:\d{2}:\d{2})");
  REQUIRE(std::regex_match(result, hms_pattern));
}

TEST_CASE("format_current_time_hms returns valid time range", "[platform][time]")
{
  auto result = radix_relay::platform::format_current_time_hms();

  constexpr auto expected_length = 8;
  constexpr auto hours_offset = 0;
  constexpr auto minutes_offset = 3;
  constexpr auto seconds_offset = 6;
  constexpr auto component_length = 2;

  REQUIRE(result.length() == expected_length);

  auto hours = std::stoi(result.substr(hours_offset, component_length));
  auto minutes = std::stoi(result.substr(minutes_offset, component_length));
  auto seconds = std::stoi(result.substr(seconds_offset, component_length));

  CHECK(hours >= 0);
  CHECK(hours <= 23);
  CHECK(minutes >= 0);
  CHECK(minutes <= 59);
  CHECK(seconds >= 0);
  CHECK(seconds <= 59);
}
