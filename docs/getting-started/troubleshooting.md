# Troubleshooting

Common issues and solutions for Radix Relay.

!!! note "Work in Progress"
    Troubleshooting guide is being developed. Check back soon.

## Getting Help

If you encounter issues:

1. Check the [GitHub Issues](https://github.com/dustingooding/radix-relay/issues)
2. Search for similar problems
3. Create a new issue with:
   - OS and version
   - Compiler and version
   - CMake version
   - Full error output
   - Steps to reproduce

## Common Issues

### Build Failures

Check that you have:
- CMake 3.21+
- C++20 compatible compiler
- All dependencies installed

See [Installation](installation.md) for details.

### Test Failures

Run tests with verbose output:
```bash
ctest --preset=test-unixlike-clang-debug --output-on-failure
```
