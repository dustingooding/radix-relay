# C++ API Reference

The C++ API documentation is generated using Doxygen and provides detailed information about all public classes, functions, and interfaces in Radix Relay.

## Browse Documentation

**[View Full C++ API Documentation →](../cpp/index.html)**

## Quick Links

- [All Classes](../cpp/annotated.html)
- [All Namespaces](../cpp/namespaces.html)
- [All Files](../cpp/files.html)
- [Class Hierarchy](../cpp/hierarchy.html)

## Main Components

### Core
Event system, async queues, CLI utilities, and command processing.

### Signal
Signal Protocol C++ bridge and FFI interface to Rust implementation.

### Nostr
Nostr transport implementation including WebSocket handling and protocol events.

### Transport
WebSocket transport abstraction layer.

### Platform
Platform-specific utilities and environment helpers.

### TUI
Terminal user interface components.

## Building Locally

To generate the C++ API documentation locally:

```bash
# Using CMake
cmake --build --preset=unixlike-clang-debug --target docs-cpp

# Or directly with Doxygen
doxygen Doxyfile
```

The generated documentation will be in `out/doxygen/html/`.
