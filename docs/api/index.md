# API Documentation

Radix Relay provides comprehensive API documentation for both C++ and Rust components.

## C++ API

The C++ API implements the transport layer, Nostr protocol, session orchestration, and user interface components.

[Browse C++ API Documentation →](cpp-api.md)

## Rust API

The Rust API implements the Signal Protocol with cryptographic primitives, key management, and session handling.

[Browse Rust API Documentation →](rust-api.md)

## Integration

The C++ and Rust components integrate via a Foreign Function Interface (FFI) bridge, with C++ consuming the Rust-implemented Signal Protocol functionality.

See the [Architecture](../architecture/index.md) documentation for more details on how these components work together.

## Building Documentation

Generate all documentation locally:

```bash
# Build everything (C++, Rust, and narrative docs)
cmake --build --preset=unixlike-clang-debug --target docs

# Or build individual components
cmake --build --preset=unixlike-clang-debug --target docs-cpp
cmake --build --preset=unixlike-clang-debug --target docs-rust
cmake --build --preset=unixlike-clang-debug --target docs-mkdocs
```
