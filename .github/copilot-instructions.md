# Exorcist IDE — Project Guidelines

Exorcist is a fast, lightweight, cross-platform Qt 6–based open-source IDE targeting Visual Studio–level capabilities with minimal footprint. Every contribution must preserve modularity, portability, and architectural clarity.

## Design Philosophy

The editor must always stay **lightweight**. Users who don't need a module must never pay its memory or startup cost. The modular plugin system gives users maximum freedom to compose exactly the environment they need — enable what they use, disable everything else. No wasted resources, no unnecessary complexity.

## Architecture

- **Interface-first design**: All core systems (`IFileSystem`, `IProcess`, `ITerminal`, `INetwork`) are abstract interfaces in `src/core/`. Never depend on concrete implementations outside the core layer.
- **Plugin-first extension model**: New features belong in plugins unless they are fundamental OS abstractions. Plugins load via `QPluginLoader` and receive services through `ServiceRegistry`.
- **Service registry for loose coupling**: Services are registered/resolved by string key. Plugins must not reference each other directly — communicate through services.
- **AI as a plugin**: AI providers implement `IAIProvider` (see `src/aiinterface.h`). Core code must never hard-code an AI backend.
- **Layered architecture**: Core abstractions → Plugin system → UI layer. Dependencies flow downward only.

See [docs/plugin_api.md](docs/plugin_api.md) for plugin development and [docs/ai.md](docs/ai.md) for AI integration strategy.

## Core vs Plugin Boundary (STRICT)

The IDE executable (`src/`) contains everything required for a fully functional code editor — editor, language services, terminal, build, search, source control, and project management. These core subsystems expose **interfaces** so that plugins can interact with any part of the IDE. Plugins are optional extensions that users can enable or disable at runtime.

### What belongs in `src/` (Core)

The core IDE — all subsystems below ship as one executable and work together:

| Layer | Contents | Examples |
|-------|----------|---------|
| **App shell** | Window, tabs, dock framework, menus, status bar | `mainwindow.*`, `main.cpp` |
| **Editor** | Text editing, syntax highlighting, piece table | `editor/` |
| **Language Intelligence** | LSP client, Clangd manager, completion, hover, diagnostics | `lsp/` |
| **Terminal** | Terminal emulator (ConPTY/PTY), VT100+ emulation | `terminal/` |
| **Build System** | Task runner, build profiles, problem matchers | `build/` |
| **Source Control** | Git integration, blame, diff, staging | `git/` |
| **Search** | Workspace search, file search, regex search | `search/` |
| **Project** | Solution/project tree model, workspace management | `project/` |
| **MCP** | Model Context Protocol client for tool servers | `mcp/` |
| **Core abstractions** | OS interfaces, file system, process, network | `core/`, `plugininterface.h`, `aiinterface.h` |
| **Plugin system** | Plugin loader, service registry | `pluginmanager.*`, `serviceregistry.*` |
| **UI framework** | Command palette, theme engine, keymap | `commandpalette.*`, `thememanager.*`, `ui/` |
| **Logger** | Application logging | `logger.*` |
| **Agent framework** | Agent orchestrator, controller, interfaces, tools, chat UI | `agent/` (runtime + interfaces, **no providers**) |

Each core subsystem exposes interfaces (registered in `ServiceRegistry`) so plugins can:
- Query diagnostics, request completions, trigger formatting (via LSP interfaces)
- Read terminal output, run commands (via terminal interfaces)
- Get git status, diffs, blame (via git interfaces)
- Access search results, file listings (via search interfaces)
- Read build output, trigger builds (via build interfaces)

### What belongs in `plugins/` (Modular)

Optional extensions that users can install, enable, or disable independently:

| Category | Plugin | Status |
|----------|--------|--------|
| **AI Providers** | Copilot, Claude, Codex, Ollama, BYOK | ✅ Already plugins |
| **Future extensions** | Additional language servers, debuggers, formatters, linters, themes, etc. | Designed as plugins from the start |

### Rules

1. **No concrete AI provider in core.** `src/` must never contain a class that implements `IAgentProvider`. All AI providers live in `plugins/`.
2. **No `#include` of plugin headers in core.** Core code references only interfaces (`IPlugin`, `IAgentPlugin`, `IAgentProvider`), never concrete plugin classes.
3. **Communication through services.** Core subsystems register their interfaces in `ServiceRegistry`. Plugins resolve services by string key — never by direct reference.
4. **Plugin-provided UI.** Plugins can contribute dock widgets, settings pages, toolbar items, and menu actions through extension interfaces (`IAgentSettingsPageProvider`, etc.).
5. **Graceful absence.** The IDE must launch and function as a text editor even if zero plugins are loaded. No crashes, no missing features beyond what the plugins provide.
6. **One plugin, one directory.** Each plugin has its own directory under `plugins/` with its own `CMakeLists.txt`. Shared utilities go in `plugins/common/`.
7. **Core subsystems are not plugins.** Editor, LSP, terminal, git, build, search, project, and MCP are integral parts of the IDE. They live in `src/` and are always available. New features in these areas go directly into `src/`.

## Code Style

- **C++17** standard, compiled with CMake 3.21+.
- **Classes**: PascalCase (`MainWindow`, `ServiceRegistry`).
- **Interfaces**: PascalCase with `I` prefix (`IFileSystem`, `IPlugin`).
- **Member variables**: `m_` prefix (`m_fileModel`, `m_services`).
- **Header guards**: `#pragma once` — no `#ifndef` guards.
- **Ownership (STRICT)**: Raw `new`/`delete` is **forbidden**. Use `std::unique_ptr` or `std::shared_ptr` for non-Qt objects; Qt parent/child ownership for QObjects. Every heap allocation must have a clear, automatic owner. No manual memory management — no dangling pointers, no leaks, no ambiguous ownership. The only acceptable `new` is when constructing a `QObject` with an explicit parent (e.g., `new QLabel(parent)`), because Qt's parent/child tree guarantees cleanup.
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
