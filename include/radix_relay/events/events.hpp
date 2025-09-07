#ifndef RADIX_RELAY_EVENTS_HPP
#define RADIX_RELAY_EVENTS_HPP

#include <concepts>
#include <string>

namespace radix_relay::events {

struct help
{
};
struct peers
{
};
struct status
{
};
struct sessions
{
};
struct scan
{
};
struct version
{
};

struct mode
{
  std::string new_mode;
};

struct send
{
  std::string peer;
  std::string message;
};

struct broadcast
{
  std::string message;
};

struct connect
{
  std::string relay;
};

struct trust
{
  std::string peer;
};

struct verify
{
  std::string peer;
};

struct raw_command
{
  std::string input;
};

// Command concept for type safety - these are parsed user commands
template<typename T>
concept Command =
  std::same_as<T, help> || std::same_as<T, peers> || std::same_as<T, status> || std::same_as<T, sessions>
  || std::same_as<T, scan> || std::same_as<T, version> || std::same_as<T, mode> || std::same_as<T, send>
  || std::same_as<T, broadcast> || std::same_as<T, connect> || std::same_as<T, trust> || std::same_as<T, verify>;

// Event concept - includes both commands and raw input
template<typename T>
concept Event = Command<T> || std::same_as<T, raw_command>;

}// namespace radix_relay::events

#endif
