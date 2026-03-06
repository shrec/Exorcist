# Exorcist IDE — Project Guidelines

Exorcist is a fast, lightweight, cross-platform Qt 6–based open-source IDE targeting Visual Studio–level capabilities with minimal footprint. Every contribution must preserve modularity, portability, and architectural clarity.

## Architecture

- **Interface-first design**: All core systems (`IFileSystem`, `IProcess`, `ITerminal`, `INetwork`) are abstract interfaces in `src/core/`. Never depend on concrete implementations outside the core layer.
- **Plugin-first extension model**: New features belong in plugins unless they are fundamental OS abstractions. Plugins load via `QPluginLoader` and receive services through `ServiceRegistry`.
- **Service registry for loose coupling**: Services are registered/resolved by string key. Plugins must not reference each other directly — communicate through services.
- **AI as a plugin**: AI providers implement `IAIProvider` (see `src/aiinterface.h`). Core code must never hard-code an AI backend.
- **Layered architecture**: Core abstractions → Plugin system → UI layer. Dependencies flow downward only.

See [docs/plugin_api.md](docs/plugin_api.md) for plugin development and [docs/ai.md](docs/ai.md) for AI integration strategy.

## Code Style

- **C++17** standard, compiled with CMake 3.21+.
- **Classes**: PascalCase (`MainWindow`, `ServiceRegistry`).
- **Interfaces**: PascalCase with `I` prefix (`IFileSystem`, `IPlugin`).
- **Member variables**: `m_` prefix (`m_fileModel`, `m_services`).
- **Header guards**: `#pragma once` — no `#ifndef` guards.
- **Ownership**: `std::unique_ptr` for non-Qt objects; Qt parent/child for QObjects.
- **Includes**: Minimal in headers; prefer forward declarations. Group: Qt headers → std headers → project headers.
- **Error reporting**: Output `QString *error` parameters for fallible operations (see `IFileSystem`).
- **User-facing strings**: Wrap in `tr()` for translation support.
- **Platform code**: Use `Q_OS_WIN`, `Q_OS_MAC`, `Q_OS_UNIX` guards. Never use `#ifdef _WIN32` style.

## Build and Test

```bash
cmake --preset default        # Configure (Ninja, Debug)
cmake --build build           # Build
./build/exorcist              # Run
```

- Only required dependency: **Qt 6 Widgets**.
- Optional dependency stubs: `EXORCIST_USE_LLVM` CMake option.
- New dependencies must be MIT/BSD/public-domain compatible. Update [docs/dependencies.md](docs/dependencies.md) when adding any.

## Conventions

- One interface, one header. Implementation in a separate `.cpp` with matching name (`ifilesystem.h` → `qtfilesystem.cpp`).
- Keep `main.cpp` minimal: app setup, theme, plugin loading, window show.
- Dock widgets in `MainWindow` are placeholders until a plugin replaces them.
- Follow the phased roadmap in [docs/roadmap.md](docs/roadmap.md) — do not skip phases.
- Commit messages: imperative mood, max 72 chars subject (`Add file search`, not `Added file search`).
- No `using namespace` in headers. Acceptable in `.cpp` files for `Qt` namespaces only.
