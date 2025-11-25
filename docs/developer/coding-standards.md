# Coding Standards

Code style and conventions for Radix Relay.

## General Principles

1. **Readability First** - Prefer clarity over cleverness
2. **Maintainability** - Code should be easy to modify
3. **Testability** - Design for testing
4. **Simplicity** - Keep it simple

## C++ Standards

### Style Guide

Follow [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)

### Modern C++

- Use C++20 features
- Prefer standard library over custom code
- Use trailing return types
- Use `auto` where type is obvious

### Memory Management

- Prefer references over pointers
- Use smart pointers when ownership is needed
- Avoid raw `new`/`delete`

### Naming Conventions

All identifiers use **snake_case** (enforced by clang-tidy):

```cpp
// Classes and structs: lower_case
class signal_bridge { };
struct message_handler { };

// Functions and methods: lower_case
auto process_message() -> void;

// Variables: lower_case
int message_count = 0;

// Enums and enum constants: lower_case
enum class transport_type { nostr, ble };

// Type aliases: lower_case with _t suffix
using session_id_t = std::string;

// Namespaces: lower_case
namespace radix_relay::core { }
```

### Formatting and Linting

Run code quality checks using the `quality` CMake target:

```bash
# Run clang-tidy on all sources
cmake --build --preset=unixlike-clang-debug --target quality
```

This runs `clangd-tidy` which applies clang-format and clang-tidy checks configured in:
- [.clang-format](.clang-format) - Code formatting rules
- [.clang-tidy](.clang-tidy) - Static analysis configuration

## Rust Standards

### Style Guide

Follow [Rust Style Guide](https://doc.rust-lang.org/beta/style-guide/)

### Idioms

- Use the type system to prevent errors
- Prefer `Result` over panics
- Use `Option` instead of null
- Implement traits for common operations

### Formatting

Uses `rustfmt` with project configuration:
```bash
cargo fmt
```

### Linting

Uses `clippy` for additional checks:
```bash
cargo clippy
```

## Git Practices

### Commit Messages

Use [gitmoji](https://gitmoji.dev/) prefixes:

```
:sparkles: add new feature
:bug: fix issue with X
:memo: update documentation
:recycle: refactor code
:white_check_mark: add tests
```

### Branch Names

```
feature/description
bugfix/issue-number
docs/topic
```

### Pull Requests

1. One feature per PR
2. Include tests
3. Update documentation
4. Pass CI checks

## Documentation

### Code Comments

- Explain **why**, not **what**
- Comment complex logic
- Use doc comments for public APIs

### C++ Documentation

```cpp
/// Brief description
///
/// Detailed description
/// @param name Parameter description
/// @return Return value description
auto function(int name) -> int;
```

### Rust Documentation

```rust
/// Brief description
///
/// Detailed description
///
/// # Arguments
/// * `name` - Parameter description
///
/// # Returns
/// Return value description
pub fn function(name: i32) -> i32 {
}
```

## Code Review

All changes require code review:

1. Check for correctness
2. Verify tests exist and pass
3. Ensure documentation is updated
4. Confirm style compliance
5. Look for potential improvements
