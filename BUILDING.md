# Building Exorcist

## Requirements

| Dependency | Version | Required | Notes |
|------------|---------|----------|-------|
| **Qt 6** | 6.5+ | Yes | Only Qt Widgets and Qt Network modules |
| **CMake** | 3.21+ | Yes | |
| **Ninja** | 1.10+ | Recommended | Default generator for CMake presets |
| **C++17 compiler** | Clang 12+ / GCC 10+ / MSVC 2019+ | Yes | |
| **LLVM** | 15+ | No | Optional — enables enhanced C++ analysis |
| **LuaJIT** | 2.1 | Bundled | Built automatically from `third_party/luajit/` |
| **Git** | 2.0+ | Yes | For submodule checkout |

## Quick Start

```bash
git clone --recurse-submodules https://github.com/shrec/Exorcist.git
cd Exorcist
cmake --preset llvm-clang
cmake --build build-llvm
```

The executable is at `build-llvm/src/exorcist` (Linux/macOS) or `build-llvm\src\exorcist.exe` (Windows).

## Platform-Specific Instructions

### Windows (LLVM MinGW)

The recommended Windows toolchain is **LLVM MinGW** shipped with Qt.

```powershell
# Configure (uses Ninja + Clang from Qt's LLVM MinGW)
cmake --preset llvm-clang

# Build
cmake --build build-llvm

# Run
.\build-llvm\src\exorcist.exe
```

If Qt is not on your PATH, set `CMAKE_PREFIX_PATH`:

```powershell
cmake --preset llvm-clang -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/llvm-mingw_64"
```

### Windows (MSVC)

```powershell
# Open a Visual Studio Developer Command Prompt, then:
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
cmake --build build
```

### Linux

```bash
# Install Qt 6 development packages (Ubuntu/Debian)
sudo apt install qt6-base-dev cmake ninja-build

# Configure and build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run
./build/src/exorcist
```

### macOS

```bash
# Install Qt 6 via Homebrew
brew install qt6 cmake ninja

# Configure and build
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)"
cmake --build build

# Run
./build/src/exorcist
```

## Build Options

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `EXORCIST_USE_LLVM` | OFF | Enable LLVM-based C++ analysis |
| `EXORCIST_DEPLOY_QT` | OFF | Run `windeployqt` after build (Windows only) |

### Building with LLVM Support

```bash
cmake -S . -B build -G Ninja \
    -DEXORCIST_USE_LLVM=ON \
    -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cmake --build build
```

## Project Structure

```
src/                  # Core IDE (single executable)
├── editor/           # Editor engine, syntax highlighting
│   └── languages/    # Per-language highlighting rules (41 languages)
├── agent/            # AI agent framework
├── lsp/              # Language Server Protocol client
├── terminal/         # Integrated terminal (PTY backend)
├── git/              # Git integration
├── search/           # Workspace search
├── build/            # Build system integration
├── mcp/              # MCP client
├── debug/            # Debug adapter framework
├── remote/           # SSH/remote development
├── project/          # Project/solution management
├── ui/               # Shared UI components
├── core/             # OS abstraction interfaces
├── sdk/              # Plugin SDK and C ABI bridge
└── plugin/           # Plugin contribution interfaces

plugins/              # Optional AI provider plugins
├── claude/           # Anthropic Claude
├── codex/            # OpenAI Codex
├── copilot/          # GitHub Copilot
└── common/           # Shared plugin utilities

third_party/
└── luajit/           # LuaJIT scripting engine (git submodule)
```

## Troubleshooting

**CMake can't find Qt**: Set `CMAKE_PREFIX_PATH` to your Qt installation directory.

**LuaJIT submodule missing**: Run `git submodule update --init --recursive`.

**Ninja not found**: Install Ninja (`pip install ninja`, `brew install ninja`, or `apt install ninja-build`), or use a different generator: `cmake -G "Unix Makefiles"`.
