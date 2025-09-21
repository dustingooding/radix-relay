#pragma once

#include <string>

#ifdef _WIN32
#include <cstdlib>
#endif

namespace radix_relay::platform {

inline auto get_home_directory() -> std::string
{
#ifdef _WIN32
  char *home = nullptr;
  size_t len = 0;
  if (_dupenv_s(&home, &len, "USERPROFILE") == 0 && home != nullptr) {
    std::string result(home);
    free(home);
    return result;
  }
  return "";
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe) - single-threaded main function
  auto *home = std::getenv("HOME");
  return home != nullptr ? std::string(home) : "";
#endif
}

inline auto get_temp_directory() -> std::string
{
#ifdef _WIN32
  char *temp = nullptr;
  size_t len = 0;
  if (_dupenv_s(&temp, &len, "TEMP") == 0 && temp != nullptr) {
    std::string result(temp);
    free(temp);
    return result;
  }
  return "C:\\temp";
#else
  auto *temp = std::getenv("TMPDIR");
  if (temp != nullptr) { return std::string(temp); }
  return "/tmp";
#endif
}

inline auto expand_tilde_path(const std::string &path) -> std::string
{
  if (!path.starts_with("~/")) { return path; }

  auto home = get_home_directory();
  if (home.empty()) { return path; }

  return home + path.substr(1);
}

}// namespace radix_relay::platform
