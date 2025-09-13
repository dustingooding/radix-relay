# Integration tests that run the actual radix-relay executable
# These are smoke tests to verify the binary works end-to-end

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND radix-relay --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND radix-relay --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

# Future integration tests can be added here:
# - Test invalid arguments return proper exit codes
# - Test configuration file loading
# - Test signal handling
# - Test network connectivity scenarios
# - Test error message formatting
