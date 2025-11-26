# Docker Instructions

If you have [Docker](https://www.docker.com/) installed, you can run this
in your terminal, when the Dockerfile is inside the `.devcontainer` directory:

```bash
docker build -f ./.devcontainer/Dockerfile --tag=radix-relay:latest .
docker run -it radix-relay:latest
```

This command will put you in a `bash` session in an Arch Linux Docker container,
with all of the tools listed in the [Dependencies](installation.md) section already installed.
The container includes:

- **Compilers**: GCC 14 and Clang 19 (matching CI configuration)
- **Build Tools**: CMake 3.21+, Ninja, ccache
- **Rust**: Stable toolchain with cargo, rustc, and rustup
- **C++ Libraries**: Boost 1.89.0 (pre-installed; other libraries like fmt, spdlog, Catch2, nlohmann-json are downloaded by CPM during first build)
- **System Libraries**: Protocol Buffers, OpenSSL
- **Development Tools**: clang-tidy, clang-format, cppcheck, gcovr, gdb, doxygen
- **Editors**: neovim, nano

The GCC_VER and LLVM_VER build arguments are defined but not actively used (Arch provides the latest stable versions).

The CC and CXX environment variables are set to GCC version 14 by default.
If you wish to use clang as your default CC and CXX environment variables, you
may do so like this:

```bash
docker build -f ./.devcontainer/Dockerfile --tag=radix-relay:latest --build-arg USE_CLANG=1 .
```

By default, you will be logged in as the `vscode` user (non-root) for better security.
You will be in a directory that contains a copy of `radix-relay`;
any changes you make to your local copy will not be updated in the Docker image
until you rebuild it.
If you need to mount your local copy directly in the Docker image, see
[Docker volumes docs](https://docs.docker.com/storage/volumes/).
TLDR:

```bash
docker run -it \
  -v /path/to/radix-relay:/workspace/radix-relay \
  radix-relay:latest
```

## Building with Docker

You can configure and [build](building.md) using these commands:

```bash
cmake --preset=unixlike-gcc-debug
cmake --build --preset=unixlike-gcc-debug
```

You can configure and build using `clang`, without rebuilding the container,
with these commands:

```bash
cmake --preset=unixlike-clang-debug
cmake --build --preset=unixlike-clang-debug
```

## Running Tests

Use `ctest` to run all tests (NEVER use `cargo test` directly):

```bash
ctest --preset=test-unixlike-gcc-debug
```

or with clang:

```bash
ctest --preset=test-unixlike-clang-debug
```

## Interactive Configuration

The `ccmake` tool is also installed; you can substitute `ccmake` for `cmake` to
configure the project interactively.
All of the tools this project supports are installed in the Docker image;
enabling them is as simple as flipping a switch using the `ccmake` interface.
Be aware that some of the sanitizers conflict with each other, so be sure to
run them separately.

## VS Code Dev Container

This project also includes a [devcontainer.json](.devcontainer/devcontainer.json) configuration
for use with VS Code's [Dev Containers extension](https://code.visualstudio.com/docs/devcontainers/containers).
Simply open the project in VS Code and click "Reopen in Container" when prompted.
