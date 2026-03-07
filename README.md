<p align="center">
  <img src="src/app.ico" alt="Exorcist IDE" width="128" height="128">
</p>

<h1 align="center">Exorcist IDE</h1>

<p align="center">
  <strong>A fast, lightweight, cross-platform code editor and IDE built with C++ and Qt 6</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#getting-started">Getting Started</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#plugins">Plugins</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#license">License</a>
</p>

---

## What is Exorcist?

Exorcist is an **open-source IDE** written in **C++17 with Qt 6** that aims to deliver Visual Studio Code–level developer experience — AI assistant, LSP intelligence, Git integration, terminal emulator — in a **native desktop application** with minimal memory footprint and instant startup.

Unlike Electron-based editors, Exorcist compiles to a single native binary with no JavaScript runtime overhead. It starts in under a second and stays responsive even with large codebases.

## Features

### Editor
- **Syntax highlighting** for 30+ languages (C/C++, Python, JavaScript, TypeScript, Rust, Go, C#, Java, Kotlin, Swift, Ruby, PHP, Dart, Lua, SQL, and more)
- **Large file support** — lazy chunked loading for files of any size
- **Minimap** sidebar for quick navigation
- **Breadcrumb** navigation bar
- **Find & Replace** with regex, case sensitivity, and whole-word matching
- **Command palette** (Ctrl+P file search, Ctrl+Shift+P commands)
- **Multi-cursor editing** support

### AI Assistant
- **Built-in AI chat panel** with streaming responses and markdown rendering
- **Inline completions** — ghost text suggestions as you type (like GitHub Copilot)
- **Inline chat** — select code and ask AI directly in the editor (Ctrl+I)
- **Next-edit suggestions** — AI predicts your next edit location
- **Multiple AI providers**: Claude, OpenAI Codex, GitHub Copilot, custom BYOK endpoints
- **15+ built-in agent tools**: file read/write, search, terminal, diagnostics, and more
- **MCP (Model Context Protocol)** server support for extensible tool discovery
- **Session persistence** — resume AI conversations across restarts
- **Context builder** — automatic workspace, git status, and terminal context

### Language Intelligence (LSP)
- **clangd integration** for C/C++ (auto-managed lifecycle)
- **Auto-completion** popup with trigger characters
- **Go to Definition** (F12) and **Find References** (Shift+F12)
- **Rename Symbol** across files
- **Hover tooltips** with documentation
- **Real-time diagnostics** — inline error/warning underlines and gutter markers
- **Code formatting** (Ctrl+Shift+F)

### Terminal
- **Integrated terminal** with full PTY backend (ConPTY on Windows, forkpty on Unix)
- **VT100/xterm emulation** — colors, cursor movement, alternate screen buffer
- **Multiple terminal tabs**
- **AI-powered terminal** — run commands suggested by the AI assistant

### Git Integration
- **File status overlays** on project tree (modified, added, deleted, untracked)
- **Inline diff gutter** — added/modified/removed line indicators
- **Diff viewer panel** — side-by-side diffs
- **Stage, unstage, commit** from the Git panel
- **Branch switching** and status bar indicator
- **AI commit message generation** — generate conventional commit messages from diffs
- **Merge conflict resolution** with AI assistance

### Search
- **Full-text search** across workspace with regex support
- **Fuzzy file search** (Ctrl+P)
- **Symbol search** with indexed outline
- **Workspace indexer** — background file scanning with chunked indexing
- **Semantic search** with TF-IDF embedding index

### Project Management
- **Solution/project tree** with file system model
- **Multi-project workspace** support
- **File watcher** — auto-detect external file changes

### Extensibility
- **Plugin architecture** — extend via Qt plugins loaded at runtime
- **Service registry** — loose coupling between components
- **AI provider plugins** — add new AI backends without modifying core
- **MCP tool discovery** — dynamically register tools from MCP servers

## Getting Started

### Requirements

| Dependency | Version | Required |
|------------|---------|----------|
| Qt 6       | 6.5+    | ✅        |
| CMake      | 3.21+   | ✅        |
| C++17 compiler | GCC 10+ / Clang 12+ / MSVC 2019+ | ✅ |
| LLVM       | 15+     | ❌ Optional |

### Build from Source

```bash
# Clone
git clone https://github.com/shrec/Exorcist.git
cd Exorcist

# Configure and build
cmake --preset default        # Ninja, Debug
cmake --build build

# Run
./build/exorcist              # Linux/macOS
.\build\exorcist.exe          # Windows
```

### Build with LLVM (Optional)

```bash
cmake -S . -B build -DEXORCIST_USE_LLVM=ON -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cmake --build build
```

### Prebuilt Binaries

> Coming soon. Check [Releases](https://github.com/shrec/Exorcist/releases) for updates.

## Architecture

Exorcist follows a **layered, interface-first architecture**:

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
│  IFileSystem · IProcess · ITerminal · INetwork │
└─────────────────────────────────────────────┘
```

**Key design principles:**
- Dependencies flow **downward only** — UI → Plugins → Services → Core
- All OS abstractions are **interfaces** in `src/core/`
- AI is a **plugin** — core never hard-codes an AI backend
- Services communicate through the **ServiceRegistry**, not direct references
- **No raw `new`/`delete`** — Qt parent/child ownership or smart pointers

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
├── build/          # Build output panel
└── ui/             # Shared UI components (notifications, etc.)
plugins/
├── claude/         # Anthropic Claude AI provider
├── codex/          # OpenAI Codex AI provider
├── copilot/        # GitHub Copilot AI provider
└── common/         # Shared plugin utilities (SSE parser, etc.)
docs/               # Architecture docs, roadmap, plugin API guide
```

## Plugins

Exorcist supports **Qt-based plugins** that extend functionality at runtime.

### Creating a Plugin

1. Implement `IPlugin` (see [docs/plugin_api.md](docs/plugin_api.md))
2. Optionally implement `IAgentPlugin` to provide AI capabilities
3. Build as a shared library and place in the `plugins/` directory
4. Exorcist loads it automatically on startup

### Built-in AI Providers

| Provider | Status | Description |
|----------|--------|-------------|
| Claude   | ✅ Ready | Anthropic Claude via HTTP streaming |
| Codex    | ✅ Ready | OpenAI Codex/GPT models |
| Copilot  | ✅ Ready | GitHub Copilot with OAuth authentication |
| Custom   | ✅ Ready | Bring Your Own Key — any OpenAI-compatible endpoint |

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+P | Quick file open |
| Ctrl+Shift+P | Command palette |
| Ctrl+F | Find in file |
| Ctrl+Shift+F | Format document |
| F12 | Go to definition |
| Shift+F12 | Find references |
| F2 | Rename symbol |
| Ctrl+Space | Trigger completion |
| Ctrl+I | Inline chat |
| Alt+\ | Trigger AI completion |
| Ctrl+` | Toggle terminal |

## Documentation

| Document | Description |
|----------|-------------|
| [docs/roadmap.md](docs/roadmap.md) | Development phases and progress |
| [docs/plugin_api.md](docs/plugin_api.md) | Plugin development guide |
| [docs/ai.md](docs/ai.md) | AI integration architecture |
| [docs/architecture.md](docs/architecture.md) | System architecture overview |
| [docs/lsp_lifecycle.md](docs/lsp_lifecycle.md) | LSP client lifecycle |
| [docs/search_framework.md](docs/search_framework.md) | Search system design |
| [docs/dependencies.md](docs/dependencies.md) | Dependency notes |
| [docs/remote_build.md](docs/remote_build.md) | Remote build setup |

## Comparison

| Feature | Exorcist | VS Code | Sublime Text | Neovim |
|---------|----------|---------|--------------|--------|
| Language | C++/Qt | Electron/JS | C++/custom | C |
| Startup time | <1s | 2-5s | <1s | <0.5s |
| Memory (idle) | ~50 MB | ~300 MB | ~80 MB | ~30 MB |
| AI assistant | Built-in | Extension | Extension | Plugin |
| LSP support | ✅ | ✅ | ✅ (LSP plugin) | ✅ |
| Integrated terminal | ✅ (native PTY) | ✅ (node-pty) | ❌ | ✅ |
| Plugin system | Qt plugins | JS extensions | Python plugins | Lua/Vimscript |
| Cross-platform | ✅ | ✅ | ✅ | ✅ |
| Open source | MIT | MIT | ❌ Proprietary | Apache-2.0 |

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

```bash
# Quick setup
git clone https://github.com/shrec/Exorcist.git
cd Exorcist
cmake --preset default
cmake --build build
```

**Code style:** C++17, PascalCase classes, `m_` prefix for members, `I` prefix for interfaces, `#pragma once` headers, no raw `new`/`delete`. See [.github/copilot-instructions.md](.github/copilot-instructions.md) for full guidelines.

## Stats

- **209 source files** · **~29,000 lines of C++**
- **Qt 6 Widgets** — single required dependency
- **MIT License** — fully open source

## License

[MIT](LICENSE) © 2026 Exorcist Contributors
