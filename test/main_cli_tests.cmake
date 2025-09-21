# Integration tests that run the actual radix-relay executable
# These are smoke tests to verify the binary works end-to-end

add_test(NAME cli.has_help COMMAND radix-relay --help)

add_test(NAME cli.verbose_mode COMMAND radix-relay --verbose peers)

add_test(NAME cli.identity_flag COMMAND radix-relay --identity ${CMAKE_CURRENT_BINARY_DIR}/test.db peers)

add_test(NAME cli.mode_internet COMMAND radix-relay --mode internet peers)

add_test(NAME cli.mode_mesh COMMAND radix-relay --mode mesh peers)

add_test(NAME cli.mode_hybrid COMMAND radix-relay --mode hybrid peers)

add_test(NAME cli.version_matches COMMAND radix-relay --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

add_test(NAME cli.invalid_mode_fails COMMAND radix-relay --mode invalid)
set_tests_properties(cli.invalid_mode_fails PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli.invalid_flag_fails COMMAND radix-relay --invalid-flag)
set_tests_properties(cli.invalid_flag_fails PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli.send_without_args_fails COMMAND radix-relay send)
set_tests_properties(cli.send_without_args_fails PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli.peers_command COMMAND radix-relay peers)

add_test(NAME cli.status_command COMMAND radix-relay status)

add_test(NAME cli.send_command COMMAND radix-relay send alice "test message")

add_test(NAME cli.send_help COMMAND radix-relay send --help)

add_test(NAME cli.peers_help COMMAND radix-relay peers --help)

add_test(NAME cli.status_help COMMAND radix-relay status --help)
