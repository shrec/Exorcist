<p align="center">
  <img src="src/app.ico" alt="Exorcist IDE Logo" width="128" height="128">
</p>

<h1 align="center">Exorcist IDE</h1>

<p align="center">
  <strong>The native C++ code editor with built-in AI — fast, lightweight, open source</strong>
</p>

<p align="center">
  <em>VS Code features. Native performance. ~50 MB memory. Sub-second startup.</em>
</p>

<p align="center">
  <a href="https://github.com/shrec/Exorcist/actions"><img src="https://img.shields.io/badge/build-passing-brightgreen?style=flat-square" alt="Build"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue?style=flat-square" alt="MIT License"></a>
  <a href="https://github.com/shrec/Exorcist"><img src="https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square&logo=cplusplus" alt="C++17"></a>
  <a href="https://doc.qt.io/qt-6/"><img src="https://img.shields.io/badge/Qt-6-41CD52?style=flat-square&logo=qt" alt="Qt 6"></a>
  <a href="https://github.com/shrec/Exorcist"><img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square" alt="Platform"></a>
</p>

<p align="center">
  <a href="#-why-exorcist">Why Exorcist?</a> •
  <a href="#-features">Features</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-comparison">Comparison</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-plugins">Plugins</a> •
  <a href="#-contributing">Contributing</a>
</p>

---

## Why Exorcist?

Modern IDEs are powerful but heavy. **VS Code** runs an entire Chromium browser. **JetBrains** IDEs need gigabytes of RAM. Developers who want AI-powered intelligence, LSP completion, Git integration, and a terminal emulator shouldn't have to pay for it with 500 MB of memory and 5-second startup times.

**Exorcist** is a ground-up rewrite of the modern IDE experience in **native C++17 with Qt 6**. It compiles to a single binary, starts instantly, and stays under 50 MB of RAM — while delivering the features you actually use every day:

- **AI chat + inline completions** — built-in, not bolted on
- **LSP intelligence** — completion, diagnostics, go-to-definition, rename
- **Integrated terminal** — native PTY, not a web terminal
- **Git panel** — status, diff, stage, commit, branch switching
- **Plugin system** — extend it with Qt plugins or C ABI / LuaJIT scripts

> **Think of it as:** VS Code's feature set in a Sublime Text–sized package, with AI built into the core architecture.

---

## Features

<table>
<tr>
<td width="50%">

### Code Editor
- Syntax highlighting for **41+ languages**
- Shebang detection for extensionless scripts
- Large file support (lazy chunked loading)
- Minimap, breadcrumb bar, multi-cursor editing
- Find & Replace with regex and whole-word matching
- Command palette (Ctrl+Shift+P)
- Fuzzy file search (Ctrl+P)

</td>
<td width="50%">

### AI Assistant
- Chat panel with streaming markdown responses
- **Inline ghost-text completions** (like Copilot)
- **Inline chat** — select code → ask AI (Ctrl+I)
- **Next-edit suggestions** — AI predicts your next edit
- 15+ built-in agent tools (file ops, search, terminal)
- MCP (Model Context Protocol) tool server support
- Session persistence across restarts

</td>
</tr>
<tr>
<td>

### Language Intelligence
- **clangd** auto-managed lifecycle for C/C++
- Auto-completion with trigger characters
- Go to Definition (F12) / Find References (Shift+F12)
- Rename Symbol across files (F2)
- Hover tooltips with documentation
- Real-time diagnostics — inline underlines + gutter markers
- Code formatting (Ctrl+Shift+F)

</td>
<td>

### Terminal & Build
- Full **native PTY** backend (ConPTY / forkpty)
- VT100/xterm emulation — colors, alternate screen
- Multiple terminal tabs
- Build profiles with problem matchers
- CMake integration and toolchain detection
- AI-powered terminal — run AI-suggested commands

</td>
</tr>
<tr>
<td>

### Git Integration
- File status overlays on project tree
- Inline diff gutter (added/modified/removed lines)
- Side-by-side diff viewer
- Stage, unstage, commit from Git panel
- Branch switching and status bar indicator
- **AI commit message generation** from diffs
- Merge conflict resolution with AI

</td>
<td>

### Search & Navigation
- Full-text workspace search with regex
- Fuzzy file search (Ctrl+P)
- Symbol search with indexed outline
- Background workspace indexer
- Semantic search with TF-IDF embedding index

</td>
</tr>
</table>

### Extensibility

- **Qt plugin system** — load/unload at runtime
- **C ABI** — write plugins in Rust, Go, Zig, or any language with C FFI
- **LuaJIT scripting** — lightweight sandboxed automation
- **Service registry** — components communicate through string-keyed services
- **AI provider plugins** — add Claude, GPT, Copilot, or your own endpoint
- **MCP protocol** — discover and register tools dynamically

---

## Quick Start

### Requirements

| Dependency | Version | Required |
|------------|---------|----------|
| **Qt 6**   | 6.5+    | Yes      |
| **CMake**  | 3.21+   | Yes      |
| **C++17 compiler** | GCC 10+ / Clang 12+ / MSVC 2019+ | Yes |
| LLVM       | 15+     | Optional |

### Build from Source

```bash
git clone https://github.com/shrec/Exorcist.git
cd Exorcist

cmake --preset default        # Configure (Ninja, Debug)
cmake --build build           # Build

./build/exorcist              # Linux/macOS
.\build\exorcist.exe          # Windows
```

### With LLVM (Optional)

```bash
cmake -S . -B build -DEXORCIST_USE_LLVM=ON -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cmake --build build
```

### Prebuilt Binaries

> Coming soon — check [Releases](https://github.com/shrec/Exorcist/releases) for updates.

---

## Comparison

How Exorcist stacks up against popular editors:

| | Exorcist | VS Code | Sublime Text | Neovim |
|---|----------|---------|--------------|--------|
| **Language** | C++17 / Qt 6 | Electron / JS | C++ / custom | C |
| **Startup** | < 1 s | 2–5 s | < 1 s | < 0.5 s |
| **Memory (idle)** | ~50 MB | ~300 MB | ~80 MB | ~30 MB |
| **AI assistant** | Built-in | Extension | Extension | Plugin |
| **LSP** | Native | Native | Plugin | Plugin |
| **Terminal** | Native PTY | node-pty | None | Native |
| **Plugin system** | Qt / C ABI / Lua | JS extensions | Python | Lua / Vim |
| **Cross-platform** | Yes | Yes | Yes | Yes |
| **Open source** | MIT | MIT | Proprietary | Apache-2.0 |

---

## Architecture

Exorcist follows a **layered, interface-first architecture** with a shared daemon (**ExoBridge**) for cross-instance services:

```
┌─────────────────────────────────────────────┐
│                   UI Layer                  │
│  MainWindow · Panels · Docks · Dialogs      │
├─────────────────────────────────────────────┤
│               Plugin System                 │
│  QPluginLoader · IPlugin · IAgentPlugin     │
├─────────────────────────────────────────────┤
│             Service Registry                │
│  Loose coupling · String-keyed resolution   │
├─────────────────────────────────────────────┤
│             Core Abstractions               │
│  IFileSystem · IProcess · ITerminal         │
├─────────────────────────────────────────────┤
│    ExoBridge (shared daemon per machine)     │
│  MCP servers · Git watch · Auth tokens      │
└─────────────────────────────────────────────┘
```

**Key design principles:**
- Dependencies flow **downward only** — UI → Plugins → Services → Core
- All OS abstractions are **interfaces** in `src/core/`
- AI is a **plugin** — core never hard-codes an AI backend
- Services communicate through the **ServiceRegistry**, not direct references
- **No raw `new`/`delete`** — smart pointers or Qt parent/child ownership
- Shared resources (MCP, git watch, auth) live in **ExoBridge**, not per-instance

### Project Structure

```
src/
├── core/           # OS abstractions (IFileSystem, IProcess, ITerminal, INetwork)
├── editor/         # EditorView, syntax highlighting, minimap, inline completions
├── agent/          # AI orchestrator, chat panel, context builder, tools
│   ├── inlinechat/ # Inline chat widget
│   └── tools/      # Built-in agent tools (file ops, search, terminal, etc.)
├── lsp/            # LSP client, clangd manager, completion popup
├── search/         # Full-text search, workspace indexer, symbol index
├── terminal/       # PTY backend, VT100 emulator, terminal panel
├── git/            # Git service, git panel, diff overlays
├── mcp/            # MCP client and panel
├── project/        # Project/solution management
├── build/          # Build system, task runner, CMake integration
├── process/        # ExoBridge IPC protocol, bridge client, process manager
├── bootstrap/      # Subsystem bootstrappers (LSP, Build, Bridge)
├── sdk/            # Stable plugin API (C++ / C ABI / LuaJIT)
└── ui/             # Shared UI (notifications, dock framework, themes)
server/             # ExoBridge daemon executable
plugins/
├── claude/         # Anthropic Claude AI provider
├── codex/          # OpenAI Codex AI provider
├── copilot/        # GitHub Copilot AI provider
└── common/         # Shared plugin utilities (SSE parser)
docs/               # Architecture docs, roadmap, plugin API guide
```

For full details see [docs/architecture.md](docs/architecture.md).

---

## Plugins

Exorcist supports runtime-loadable plugins via three interfaces:

| Interface | Language | Use case |
|-----------|----------|----------|
| **Qt C++ plugin** | C++ | Full Qt access, rich UI contribution |
| **C ABI plugin** | Rust, Go, Zig, C | Compiler-independent, stable binary interface |
| **LuaJIT script** | Lua | Lightweight automation, sandboxed |

### Built-in AI Providers

| Provider | Status | Model |
|----------|--------|-------|
| **Claude** | Ready | Anthropic Claude (HTTP streaming) |
| **Codex** | Ready | OpenAI GPT / Codex models |
| **Copilot** | Ready | GitHub Copilot (OAuth) |
| **Custom** | Ready | Any OpenAI-compatible endpoint (BYOK) |

See [docs/plugin_api.md](docs/plugin_api.md) for the plugin development guide.

---

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+P` | Quick file open |
| `Ctrl+Shift+P` | Command palette |
| `Ctrl+F` | Find in file |
| `Ctrl+Shift+F` | Format document |
| `F12` | Go to definition |
| `Shift+F12` | Find references |
| `F2` | Rename symbol |
| `Ctrl+Space` | Trigger completion |
| `Ctrl+I` | Inline chat |
| `Alt+\` | Trigger AI completion |
| `` Ctrl+` `` | Toggle terminal |

---

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](docs/architecture.md) | System design, layers, ExoBridge |
| [Development Principles](docs/development-principles.md) | How Exorcist evolves as a system |
| [Core Philosophy](docs/core-philosophy.md) | Core vs Plugin boundary, activation model |
| [Roadmap](docs/roadmap.md) | Development phases and progress |
| [Plugin API](docs/plugin_api.md) | Plugin development guide |
| [AI Integration](docs/ai.md) | AI architecture and providers |
| [LSP Lifecycle](docs/lsp_lifecycle.md) | LSP client lifecycle |
| [Search Framework](docs/search_framework.md) | Search system design |
| [Remote Build](docs/remote_build.md) | Remote build via SSH |
| [Dependencies](docs/dependencies.md) | Third-party dependency notes |

---

## Project Stats

| Metric | Value |
|--------|-------|
| Source files | **664** |
| Lines of C++ | **~96,000** |
| Languages highlighted | **41+** |
| AI agent tools | **112** |
| Plugins | **12** |
| Tests | **49** |
| Required dependency | **Qt 6 only** |
| License | **MIT** |

---

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

```bash
git clone https://github.com/shrec/Exorcist.git
cd Exorcist
cmake --preset default
cmake --build build
```

**Code style:** C++17, PascalCase classes, `m_` prefix for members, `I` prefix for interfaces, `#pragma once`, no raw `new`/`delete`. Full guidelines in [.github/copilot-instructions.md](.github/copilot-instructions.md).

---

## License

[MIT](LICENSE) &copy; 2025–2026 Exorcist Contributors
