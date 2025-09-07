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

## Command Line (Compatible with VSCode)

**Important**: Use the same preset structure as VSCode to avoid conflicts.

A full build has different steps:

1) Choose appropriate CMake preset
2) Configure using the preset
3) Build the project

## (1) Specify the compiler using environment variables

By default (if you don't set environment variables `CC` and `CXX`), the system default compiler will be used.

CMake uses the environment variables CC and CXX to decide which compiler to use. So to avoid the conflict issues only specify the compilers using these variables.

### Commands for setting the compilers

#### Debian/Ubuntu/MacOS

Set your desired compiler (`clang`, `gcc`, etc):

##### Temporarily (only for the current shell)

Run one of the following in the terminal:

- clang:

  ```bash
  CC=clang CXX=clang++
  ```

- gcc:

  ```bash
  CC=gcc CXX=g++
  ```

##### Permanent

Open `~/.bashrc` using your text editor:

```bash
gedit ~/.bashrc
```

Add `CC` and `CXX` to point to the compilers:

```bash
export CC=clang
export CXX=clang++
```

Save and close the file.

#### Windows

**Permanent:**

Run one of the following in PowerShell:

- Visual Studio generator and compiler (cl):

  ```powershell
  [Environment]::SetEnvironmentVariable("CC", "cl.exe", "User")
  [Environment]::SetEnvironmentVariable("CXX", "cl.exe", "User")
  refreshenv
  ```

  Set the architecture using [vcvarsall](https://docs.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=vs-2019#vcvarsall-syntax):

  ```cmd
  vcvarsall.bat x64
  ```

- clang:

  ```powershell
  [Environment]::SetEnvironmentVariable("CC", "clang.exe", "User")
  [Environment]::SetEnvironmentVariable("CXX", "clang++.exe", "User")
  refreshenv
  ```

- gcc:

  ```powershell
  [Environment]::SetEnvironmentVariable("CC", "gcc.exe", "User")
  [Environment]::SetEnvironmentVariable("CXX", "g++.exe", "User")
  refreshenv
  ```

**Temporarily (only for the current shell):**

```powershell
$Env:CC="clang.exe"
$Env:CXX="clang++.exe"
```

## (2) Configure using CMake presets

**Using presets ensures compatibility with VSCode and standardized build directories.**

List available presets:

```bash
cmake --list-presets=configure
```

Available presets:

- `unixlike-clang-debug` - Linux/macOS with Clang (Debug)
- `unixlike-clang-release` - Linux/macOS with Clang (Release)  
- `unixlike-gcc-debug` - Linux/macOS with GCC (Debug)
- `unixlike-gcc-release` - Linux/macOS with GCC (Release)
- `windows-msvc-debug-developer-mode` - Windows MSVC (Debug, Developer Mode)
- `windows-msvc-release-developer-mode` - Windows MSVC (Release, Developer Mode)

Configure using preset:

```bash
cmake --preset=unixlike-clang-debug
```

This automatically creates `out/build/unixlike-clang-debug/` directory structure compatible with VSCode.

### (2.a) Legacy cmake configuration (not recommended)

If you need to use legacy cmake directly without presets:

```bash
cmake -S . -B ./build
```

**Warning**: This creates a different directory structure that conflicts with VSCode CMake Tools extension.

### (2.b) Configuring via ccmake

With the Cmake Curses Dialog Command Line tool:

```bash
ccmake -S . -B ./build
```

Once `ccmake` has finished setting up, press 'c' to configure the project,
press 'g' to generate, and 'q' to quit.

### (2.c) Configuring via cmake-gui

To use the GUI of the cmake:

2.c.1) Open cmake-gui from the project directory:

```bash
cmake-gui .
```

2.c.2) Set the build directory:

![build_dir](https://user-images.githubusercontent.com/16418197/82524586-fa48e380-9af4-11ea-8514-4e18a063d8eb.jpg)

2.c.3) Configure the generator:

In cmake-gui, from the upper menu select `Tools/Configure`.

**Warning**: if you have set `CC` and `CXX` always choose the `use default native compilers` option. This picks `CC` and `CXX`. Don't change the compiler at this stage!

### Windows - MinGW Makefiles

Choose MinGW Makefiles as the generator:

![mingw](https://user-images.githubusercontent.com/16418197/82769479-616ade80-9dfa-11ea-899e-3a8c31d43032.png)

### Windows - Visual Studio generator and compiler

You should have already set `C` and `CXX` to `cl.exe`.

Choose "Visual Studio 16 2019" as the generator:

![default_vs](https://user-images.githubusercontent.com/16418197/82524696-32502680-9af5-11ea-9697-a42000e900a6.jpg)

### Windows - Visual Studio generator and Clang Compiler

You should have already set `C` and `CXX` to `clang.exe` and `clang++.exe`.

Choose "Visual Studio 16 2019" as the generator. To tell Visual studio to use `clang-cl.exe`:

- If you use the LLVM that is shipped with Visual Studio: write `ClangCl` under "optional toolset to use".

![visual_studio](https://user-images.githubusercontent.com/16418197/82781142-ae60ac00-9e1e-11ea-8c77-222b005a8f7e.png)

- If you use an external LLVM: write [`LLVM_v142`](https://github.com/zufuliu/llvm-utils#llvm-for-visual-studio-2017-and-2019) under "optional toolset to use".

![visual_studio](https://user-images.githubusercontent.com/16418197/82769558-b3136900-9dfa-11ea-9f73-02ab8f9b0ca4.png)

2.c.4) Choose the Cmake options and then generate:

![generate](https://user-images.githubusercontent.com/16418197/82781591-c97feb80-9e1f-11ea-86c8-f2748b96f516.png)

## (3) Build the project

### Using presets (recommended)

Build using the preset you configured:

```bash
cmake --build --preset=unixlike-clang-debug
```

Or use the build directory directly:

```bash
cmake --build out/build/unixlike-clang-debug
```

### Legacy build (if not using presets)

```bash
cmake --build ./build
```

For Visual Studio, specify configuration:

```bash
cmake --build ./build -- /p:configuration=Release
```

## Running the tests

### Using test presets (recommended)

```bash
ctest --preset=test-unixlike-clang-debug
```

### Using build directory directly

```bash
ctest --test-dir out/build/unixlike-clang-debug
```

### Legacy test running

```bash
cd ./build
ctest -C Debug
cd ../
```

## VSCode Integration Notes

- **Build tasks**: Use F7 or Ctrl+Shift+P → "CMake: Build"
- **Run/Debug**: Use F5 or Ctrl+F5 after selecting target
- **Test explorer**: Tests appear in VSCode Test Explorer panel
- **IntelliSense**: Automatically configured using compile_commands.json

**Important**: Always use the same preset in both VSCode and command line to avoid build cache conflicts.
