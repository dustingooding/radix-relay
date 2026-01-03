#include <platform/time_utils.hpp>

#include <chrono>
#include <ctime>
#include <fmt/format.h>

namespace radix_relay::platform {

auto format_current_time_hms() -> std::string
{
  using namespace std::chrono;

  auto now = system_clock::now();
  const std::time_t time_now = system_clock::to_time_t(now);

  std::tm time_tm{};
#if defined(_WIN32)
  std::ignore = localtime_s(&time_tm, &time_now);
#else
  std::ignore = localtime_r(&time_now, &time_tm);
#endif

  return fmt::format("{:02d}:{:02d}:{:02d}", time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec);
}

auto current_timestamp_ms() -> std::uint64_t
{
  using namespace std::chrono;
  return static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

}// namespace radix_relay::platform
