#pragma once

namespace radix_relay::core {

/**
 * @brief Helper for std::visit with overload pattern.
 *
 * Enables clean variant visiting with separate lambdas for each alternative:
 * @code
 * std::visit(overload{
 *   [](int i) { std::cout << "int: " << i; },
 *   [](std::string s) { std::cout << "string: " << s; }
 * }, variant);
 * @endcode
 */
template<class... Ts> struct overload : Ts...
{
  using Ts::operator()...;
};

template<class... Ts> overload(Ts...) -> overload<Ts...>;

}// namespace radix_relay::core
