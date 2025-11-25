# Rust API Reference

The Rust API documentation is generated using rustdoc and provides detailed information about the Signal Protocol implementation and cryptographic primitives.

## Browse Documentation

**[View Full Rust API Documentation →](../rust/signal_bridge/all.html)**

## signal_bridge Crate

The Signal Protocol implementation providing X3DH key agreement and Double Ratchet encryption with post-quantum (Kyber) support.

### Key Modules

- **identity** - Identity key management
- **prekey** - Pre-key generation and bundles
- **session** - Session establishment and management
- **crypto** - Cryptographic primitives
- **storage** - Key and session storage

Browse the [complete module documentation](../rust/signal_bridge/all.html) for detailed API reference.

## Building Locally

To generate the Rust API documentation locally:

```bash
# Using CMake
cmake --build --preset=unixlike-clang-debug --target docs-rust

# Or directly with Cargo
cargo doc --manifest-path rust/Cargo.toml --no-deps --all-features --open
```

The generated documentation will be in `rust/target/doc/`.
