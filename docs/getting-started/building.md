# Build Instructions

This project uses CMakePresets.json for standardized build configurations that work with VSCode CMake Tools extension and command line.

## Quick Start (VSCode CMake Tools)

**Recommended**: Use VSCode with the CMake Tools extension for the best development experience.

1. Open project in VSCode
2. Install CMake Tools extension if not already installed
3. Use Ctrl+Shift+P → "CMake: Select Configure Preset" → choose `unixlike-clang-debug` (Linux/macOS) or `windows-msvc-debug-developer-mode` (Windows)
4. Build with Ctrl+Shift+P → "CMake: Build" or F7

VSCode will automatically:

- Configure in `out/build/<preset-name>/`
- Install to `out/install/<preset-name>/`
- Set up debugging and testing integration

## Command Line Build

**Important**: Use the same preset structure as VSCode to avoid conflicts.

### Available Presets

List available presets:

```bash
cmake --list-presets=configure
```

Available presets:

- `unixlike-clang-debug` - Linux/macOS with Clang (Debug)
- `unixlike-clang-release` - Linux/macOS with Clang (Release)
- `unixlike-gcc-debug` - Linux with GCC (Debug)
- `unixlike-gcc-release` - Linux with GCC (Release)
- `windows-msvc-debug-developer-mode` - Windows MSVC (Debug, Developer Mode)
- `windows-msvc-release-developer-mode` - Windows MSVC (Release, Developer Mode)
- `windows-msvc-debug-user-mode` - Windows MSVC (Debug, User Mode)
- `windows-msvc-release-user-mode` - Windows MSVC (Release, User Mode)

**Note**: Windows only supports MSVC due to libsignal compatibility requirements.

### Configure with a Preset

Configure using your chosen preset:

```bash
cmake --preset=unixlike-clang-debug
```

This automatically creates `out/build/unixlike-clang-debug/` directory structure compatible with VSCode.

### Build the Project

Build using the preset you configured:

```bash
cmake --build --preset=unixlike-clang-debug
```

Or use the build directory directly:

```bash
cmake --build out/build/unixlike-clang-debug
```

## Quality Checks

### Rust Quality Checks

This project includes automated Rust quality tools that run as part of the build process:

- **rustfmt**: Code formatting (configured via `rustfmt.toml`)
- **clippy**: Linting and static analysis

#### Running Rust Quality Checks Manually

```bash
# Check code formatting (without modifying files)
cmake --build --preset=unixlike-clang-debug --target rust-fmt-check

# Apply code formatting
cmake --build --preset=unixlike-clang-debug --target rust-fmt

# Run clippy linter
cmake --build --preset=unixlike-clang-debug --target rust-clippy

# Run all quality checks
cmake --build --preset=unixlike-clang-debug --target quality-rust
```

**Note**: Rust quality tools (clippy and rustfmt) are automatically installed via `.envrc` when using direnv.

## Running the Application

After building, run Radix Relay in either GUI or TUI mode:

### GUI Mode (Default)

Modern graphical interface with terminal aesthetic (Fira Code font, green/black theme):

```bash
./out/build/unixlike-clang-debug/src/radix-relay --ui-mode gui
```

### TUI Mode

Terminal-based command-line interface:

```bash
./out/build/unixlike-clang-debug/src/radix-relay --ui-mode tui
```

Both modes provide the same functionality with different presentation styles.

## Running the Tests

Run tests using test presets:

```bash
ctest --preset=test-unixlike-clang-debug
```

Or use the build directory directly:

```bash
ctest --test-dir out/build/unixlike-clang-debug
```

## Documentation

Build and serve documentation:

```bash
# Build all documentation (C++ API, Rust API, narrative docs)
cmake --build --preset=unixlike-clang-debug --target docs

# Serve documentation locally at http://127.0.0.1:8000/
cmake --build --preset=unixlike-clang-debug --target docs-serve
```

Individual documentation targets:

```bash
# Build only C++ API documentation (Doxygen)
cmake --build --preset=unixlike-clang-debug --target docs-cpp

# Build only Rust API documentation (rustdoc)
cmake --build --preset=unixlike-clang-debug --target docs-rust

# Build only narrative documentation (MkDocs)
cmake --build --preset=unixlike-clang-debug --target docs-mkdocs

# Clean documentation output
cmake --build --preset=unixlike-clang-debug --target docs-clean
```

## VSCode Integration Notes

- **Build tasks**: Use F7 or Ctrl+Shift+P → "CMake: Build"
- **Run/Debug**: Use F5 or Ctrl+F5 after selecting target
- **Test explorer**: Tests appear in VSCode Test Explorer panel
- **IntelliSense**: Automatically configured using compile_commands.json

**Important**: Always use the same preset in both VSCode and command line to avoid build cache conflicts.
