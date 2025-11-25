# Testing

Test-driven development practices for Radix Relay.

## Testing Philosophy

Radix Relay follows BDD/TDD principles:

1. **Red** - Write a failing test
2. **Green** - Make it pass
3. **Refactor** - Clean up code

## Running Tests

### All Tests
```bash
ctest --preset=test-unixlike-clang-debug
```

### Specific Test
```bash
ctest --preset=test-unixlike-clang-debug -R signal_bridge_tests
```

### With Output
```bash
ctest --preset=test-unixlike-clang-debug --output-on-failure
```

## Test Organization

```
test/
├── *_tests.cpp           # C++ unit tests
├── test_doubles/         # Test doubles (mocks, stubs)
└── CMakeLists.txt
```

## Writing Tests

### C++ Tests (Catch2)

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Feature description", "[tag]") {
  SECTION("Specific behavior") {
    // Arrange
    auto sut = SystemUnderTest();

    // Act
    auto result = sut.doSomething();

    // Assert
    REQUIRE(result == expected);
  }
}
```

### Rust Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_feature() {
        // Arrange
        let sut = SystemUnderTest::new();

        // Act
        let result = sut.do_something();

        // Assert
        assert_eq!(result, expected);
    }
}
```

## Test Coverage

Run with coverage:
```bash
cmake --preset=unixlike-clang-debug -DENABLE_COVERAGE=ON
cmake --build --preset=unixlike-clang-debug
ctest --preset=test-unixlike-clang-debug
```

## Best Practices

1. **Test behavior, not implementation**
2. **One assertion per test** (when possible)
3. **Use descriptive test names**
4. **Keep tests fast**
5. **Make tests deterministic**
