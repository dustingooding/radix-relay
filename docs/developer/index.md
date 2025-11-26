# Developer Guide

Welcome to the Radix Relay developer documentation.

## Quick Links

- [**Contributing**](contributing.md) - How to contribute to the project
- [**Testing**](testing.md) - Test-driven development practices
- [**Coding Standards**](coding-standards.md) - Code style and conventions

## Development Workflow

### 1. Set Up Environment

```bash
# Clone repository
git clone https://github.com/dustingooding/radix-relay.git
cd radix-relay

# direnv will automatically set up the environment
direnv allow
```

### 2. Make Changes

```bash
# Create a feature branch
git checkout -b feature/my-feature

# Make your changes
# Write tests first (TDD)
# Implement feature
# Run tests
```

### 3. Test

```bash
# Run all tests
ctest --preset=test-unixlike-clang-debug

# Run specific test
ctest --preset=test-unixlike-clang-debug -R test_name
```

### 4. Submit

```bash
# Commit with gitmoji prefix
git commit -m ":sparkles: add new feature"

# Push and create PR
git push origin feature/my-feature
```

## Build System

Uses modern CMake with:

- CMake 3.21+
- Preset-based configuration
- CPM for dependency management
- Corrosion for Rust integration

## Code Organization

```
radix-relay/
├── src/              # C++ source files
├── include/          # Public headers
├── lib/              # Libraries (Signal Protocol bridge)
├── rust/             # Rust components
├── test/             # Tests
└── docs/             # Documentation
```

## Getting Help

- Check [Troubleshooting](../getting-started/troubleshooting.md)
- Open an [Issue](https://github.com/dustingooding/radix-relay/issues)
- Email: <radix-relay@proton.me>
