# Getting Started

Welcome to Radix Relay! This guide will help you get up and running quickly.

## Prerequisites

- **CMake 3.21+**
- **C++20 compiler**: Clang 19.1.1, GCC 14+, or MSVC 2022
- **Rust**: Stable (with clippy and rustfmt)
- **Protocol Buffers**: protoc 3.x+
- **Ninja**: Build system
- **Python 3.8+**: For documentation (optional)

## Quick Start Options

### Docker (Easiest)

The fastest way to get started:

```bash
docker compose up dev
```

See [Docker instructions](docker.md) for details.

### Automated Setup (setup-cpp)

Install all dependencies automatically:

```bash
# Linux/macOS
curl -sL https://github.com/aminya/setup-cpp/releases/latest/download/setup_cpp_linux -o setup_cpp
chmod +x setup_cpp
./setup_cpp --compiler llvm-19 --cmake true --ninja true --ccache true --clangtidy 19.1.1

# Then install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup component add clippy rustfmt
```

### Manual Installation

Follow the detailed [Installation guide](installation.md) for manual dependency setup.

## Quick Navigation

- [**Installation**](installation.md) - Set up dependencies and development environment
- [**Building**](building.md) - Compile the project from source
- [**Docker**](docker.md) - Run in a containerized environment
- [**Troubleshooting**](troubleshooting.md) - Common issues and solutions

## Typical Workflow

1. **Install dependencies** - See [Installation](installation.md)

2. **Clone and configure:**

   ```bash
   git clone https://github.com/dustingooding/radix-relay.git
   cd radix-relay
   cmake --preset=unixlike-clang-debug
   ```

3. **Build:**

   ```bash
   cmake --build --preset=unixlike-clang-debug
   ```

4. **Run tests:**

   ```bash
   ctest --preset=test-unixlike-clang-debug
   ```

5. **Build documentation (optional):**

   ```bash
   cmake --build --preset=unixlike-clang-debug --target docs
   ```

## What's Next?

- **New to the project?** Start with [Installation →](installation.md)
- **Ready to build?** Jump to [Building →](building.md)
- **Want to contribute?** See [Developer Guide →](../developer/contributing.md)
- **Need help?** Check [Troubleshooting →](troubleshooting.md)

[Continue to Installation →](installation.md){ .md-button .md-button--primary }
