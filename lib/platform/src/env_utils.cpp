#include <platform/env_utils.hpp>

#include <mutex>

#ifdef _WIN32
#include <cstdlib>
#include <memory>
#endif

namespace radix_relay::platform {

auto get_home_directory() -> std::string
{
#ifdef _WIN32
  char *home_raw = nullptr;
  size_t len = 0;
  if (_dupenv_s(&home_raw, &len, "USERPROFILE") == 0 and home_raw != nullptr) {
    const std::unique_ptr<char, decltype(&free)> home(home_raw, &free);
    return { home.get() };
  }
  return "";
#else
  static std::mutex env_mutex;
  const std::scoped_lock lock(env_mutex);

  auto *home = std::getenv("HOME");// NOLINT(concurrency-mt-unsafe)
  return home != nullptr ? std::string(home) : "";
#endif
}

auto get_temp_directory() -> std::string
{
#ifdef _WIN32
  char *temp_raw = nullptr;
  size_t len = 0;
  if (_dupenv_s(&temp_raw, &len, "TEMP") == 0 and temp_raw != nullptr) {
    const std::unique_ptr<char, decltype(&free)> temp(temp_raw, &free);
    return { temp.get() };
  }
  return "C:\\temp";
#else
  static std::mutex env_mutex;
  const std::scoped_lock lock(env_mutex);

  const char *temp = std::getenv("TMPDIR");// NOLINT(concurrency-mt-unsafe)
  if (temp != nullptr) { return { temp }; }
  return "/tmp";
#endif
}

auto expand_tilde_path(const std::string &path) -> std::string
{
  if (not path.starts_with("~/")) { return path; }

  auto home = get_home_directory();
  if (home.empty()) { return path; }

  return home + path.substr(1);
}

}// namespace radix_relay::platform
