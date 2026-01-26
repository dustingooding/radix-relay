# Dependencies

This guide covers the dependencies required to build Radix Relay from source.

## Quick Setup Options

### Docker (Recommended)

The easiest way to get started is using Docker, which includes all dependencies pre-configured:

```bash
docker compose up dev
```

See [Docker instructions](docker.md) for details.

### Automated Installation (setup-cpp)

For automated dependency installation, use [setup-cpp](https://github.com/aminya/setup-cpp):

**Linux/macOS:**
```bash
curl -sL https://github.com/aminya/setup-cpp/releases/latest/download/setup_cpp_linux -o setup_cpp
chmod +x setup_cpp
./setup_cpp --compiler llvm-19 --cmake true --ninja true --ccache true --clangtidy 19.1.1 --cppcheck true
```

**Windows (PowerShell as admin):**
```powershell
curl -LJO "https://github.com/aminya/setup-cpp/releases/latest/download/setup_cpp_windows.exe"
./setup_cpp_windows --compiler llvm-19 --cmake true --ninja true --ccache true --clangtidy 19.1.1 --cppcheck true
RefreshEnv.cmd
```

## Dependency Management Strategy

Radix Relay uses a multi-layered dependency management approach:

### Build Tools & Compilers (Spack - Local Development)

For local development with precise version control:

- **cmake** @3.27 - Build system
- **ninja** @1.12 - Build generator
- **llvm** @19.1 - Clang compiler toolchain
- **gcc** @13.3 - GCC compiler toolchain
- **doxygen** @1.9 - Documentation generator
- **graphviz** @2.42 - Documentation diagrams
- **pkgconf** @2.3 - Package config tool (required for SimpleBLE on Linux)

**Installation:** See spack.yaml in the project root. Spack automatically installs these when you activate the environment.

### C++ Library Dependencies (vcpkg)

Managed via vcpkg.json, automatically installed during CMake configuration:

- **protobuf** - All platforms (Protocol Buffers)
- **openssl** - Windows only (Linux/macOS use system OpenSSL)
- **sqlcipher** - Windows only (Linux/macOS use system packages)

**Installation:** Automatically handled during CMake configuration.

### Additional C++ Libraries (CPM - Automatic)

Downloaded and built automatically via CMake Package Manager (CPM):

- **SimpleBLE** (v0.6.1) - Cross-platform BLE library
- **Boost** (asio, beast, system) - Async I/O and HTTP
- **Slint** - GUI framework
- **Replxx** - TUI readline library
- **Catch2** - Testing framework
- **libsignal** (Rust) - Signal Protocol via Corrosion

**Installation:** No action required - handled by Dependencies.cmake.

### System Libraries (Platform Package Managers)

Platform-specific system dependencies:

- **OpenSSL** - Linux/macOS only (vcpkg on Windows)
- **SQLCipher** - Linux/macOS only (vcpkg on Windows)
- **D-Bus** - Linux only (for SimpleBLE BlueZ backend)

**Installation:** Use your system package manager (apt/dnf/pacman/brew).

## Manual Installation

### Required Dependencies

#### 1. C++20 Compiler

One of the following:

- **Clang 19.1.1** (recommended) or GCC 14+
- **MSVC 2022** (Windows only - required for libsignal compatibility)

**Linux (Ubuntu/Debian):**
```bash
# Clang 19
bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

# Or GCC 14
sudo apt install build-essential gcc-14 g++-14
```

**macOS:**
```bash
# Clang (via Xcode Command Line Tools)
xcode-select --install

# Or via Homebrew
brew install llvm@19
```

**Windows:**

Install Visual Studio 2022 with C++ Desktop Development workload:
```powershell
choco install visualstudio2022community --package-parameters "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
```

**Note:** Windows builds require MSVC due to libsignal compatibility. GCC/Clang are not supported on Windows.

#### 2. CMake 3.21+

**Linux (Ubuntu/Debian):**
```bash
sudo apt install cmake
```

**macOS:**
```bash
brew install cmake
```

**Windows:**
```powershell
choco install cmake
```

#### 3. Ninja Build System

**Linux (Ubuntu/Debian):**
```bash
sudo apt install ninja-build
```

**macOS:**
```bash
brew install ninja
```

**Windows:**
```powershell
choco install ninja
```

#### 4. Rust (stable)

Required for Signal Protocol implementation.

**All platforms:**
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

Or visit [rustup.rs](https://rustup.rs/)

After installation:
```bash
rustup component add clippy rustfmt
```

#### 5. Protocol Buffers

Required by Signal Protocol dependencies.

**Linux (Ubuntu/Debian):**
```bash
sudo apt install protobuf-compiler libprotobuf-dev
```

**macOS:**
```bash
brew install protobuf
```

**Windows:**

Protocol Buffers is installed automatically via vcpkg during the build process.

#### 6. BLE Transport Dependencies

Required for Bluetooth Low Energy transport functionality.

**System D-Bus (Linux only):**

SimpleBLE on Linux uses the BlueZ backend which requires D-Bus development headers. On Windows and macOS, SimpleBLE uses native Bluetooth APIs (no D-Bus needed).

```bash
# Ubuntu/Debian
sudo apt install libdbus-1-dev

# Fedora/RHEL
sudo dnf install dbus-devel

# Arch
sudo pacman -S dbus
```

**SimpleBLE (all platforms):**

SimpleBLE is automatically downloaded and built via CPM (CMake Package Manager) during configuration. No manual installation needed.

- **Version**: v0.6.1
- **Linux**: Uses BlueZ backend (requires system D-Bus installed above)
- **Windows**: Uses native Windows Bluetooth API
- **macOS**: Uses CoreBluetooth framework

**vcpkg Setup (for Protobuf):**

Install vcpkg if you haven't already:

```bash
git clone https://github.com/microsoft/vcpkg.git /data/git/vcpkg
/data/git/vcpkg/bootstrap-vcpkg.sh  # Linux/macOS
# or
git clone https://github.com/microsoft/vcpkg.git %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat  # Windows

# Set environment variable (add to your shell profile or .envrc)
export VCPKG_ROOT=/data/git/vcpkg  # Linux/macOS
# or
set VCPKG_ROOT=%USERPROFILE%\vcpkg  # Windows
```

The project's CMake presets automatically use the vcpkg toolchain for Protobuf and other dependencies.

### Optional Dependencies

#### Documentation Tools

Required only if building documentation locally:

**Linux (Ubuntu/Debian):**
```bash
sudo apt install doxygen graphviz python3-pip
pip install mkdocs mkdocs-material
```

**macOS:**
```bash
brew install doxygen graphviz python
pip3 install mkdocs mkdocs-material
```

**Windows:**
```powershell
choco install doxygen.install graphviz python
pip install mkdocs mkdocs-material
```

#### Development Tools (Optional but Recommended)

**ccache** - Speeds up recompilation:

```bash
# Linux
sudo apt install ccache

# macOS
brew install ccache

# Windows
choco install ccache
```

**cppcheck** - Static analysis:

```bash
# Linux
sudo apt install cppcheck

# macOS
brew install cppcheck

# Windows
choco install cppcheck
```

**clang-tidy 19.1.1** - Linting and static analysis (matches CI):

```bash
# Linux
sudo apt install clang-tidy-19

# macOS
brew install llvm@19

# Windows
# Included with Visual Studio 2022 LLVM toolset
```

## Version Requirements Summary

| Dependency | Minimum Version | Recommended |
|------------|----------------|-------------|
| CMake | 3.21 | Latest |
| C++ Compiler | C++20 support | Clang 19.1.1, GCC 14, or MSVC 2022 |
| Rust | stable | Latest stable |
| Protocol Buffers | 3.x | Latest |
| pkg-config (Linux BLE) | Any | Latest |
| D-Bus (Linux BLE) | 1.x | Latest |
| Python (for docs) | 3.8 | 3.12+ |

## Verification

After installation, verify all dependencies:

```bash
cmake --version          # Should be 3.21+
ninja --version         # Any version
rustc --version         # Should show stable
cargo --version         # Should match rustc
protoc --version        # Should be 3.x+

# Compiler check
clang++ --version       # 19.1.1+ (Linux/macOS)
# or
g++ --version          # 14+ (Linux)
# or
cl                     # MSVC 2022 (Windows)

# BLE dependencies (Linux only)
pkg-config --version    # Any version
pkg-config --exists dbus-1 && echo "D-Bus found" || echo "D-Bus not found"
```

## Next Steps

Once dependencies are installed, proceed to [Building](building.md).

## Package Manager Notes

- **Linux**: Commands shown for Debian/Ubuntu (apt). For other distributions:
  - Fedora/RHEL: Replace `apt` with `dnf`
  - Arch: Replace `apt` with `pacman`
- **macOS**: Requires [Homebrew](https://brew.sh/)
- **Windows**: Requires [Chocolatey](https://chocolatey.org/)

## Troubleshooting

### CMake can't find dependencies

Ensure dependency binaries are on your PATH:

```bash
# Linux/macOS
echo $PATH

# Windows (PowerShell)
$env:PATH
```

### Rust components missing

Install required Rust components:
```bash
rustup component add clippy rustfmt
```

### Protocol Buffers not found

Verify protoc is installed and on PATH:
```bash
protoc --version
which protoc  # Linux/macOS
where protoc  # Windows
```

### BLE dependencies missing (Linux)

If CMake cannot find DBus1 or pkg-config:

```bash
# Verify pkg-config is installed
pkg-config --version

# Verify D-Bus development libraries are installed
pkg-config --exists dbus-1 && echo "Found" || echo "Not found"

# If not found, install them:
sudo apt install pkg-config libdbus-1-dev  # Ubuntu/Debian
sudo dnf install pkgconfig dbus-devel      # Fedora/RHEL
sudo pacman -S pkgconf dbus                # Arch
spack install pkgconf dbus                 # Spack
```

If issues persist, see the [GitHub Issues](https://github.com/dustingooding/radix-relay/issues) or join discussions.
