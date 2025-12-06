// NOTE: Do NOT include processor.hpp here to avoid instantiating boost::system
// symbols in this shared library, which causes ODR violations with GCC + ASAN
// This is a known limitation: header-only Boost.System in multiple shared libraries
// triggers false positives that can't be suppressed at compile-time with GCC.
// See: https://github.com/google/sanitizers/issues/1017

namespace radix_relay::slint_ui {

// FIXME: Remove this dummy function once real non-template code is added
// When adding non-template member functions, be careful about which headers
// are included to avoid pulling in boost::system::error_code
auto dummy_function_for_shared_library_instantiation() -> void {}

}// namespace radix_relay::slint_ui
