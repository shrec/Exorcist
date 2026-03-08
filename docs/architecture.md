# Architecture

The goal is strict modularity, portability, and minimal footprint. The code is
organized into layers with clear dependency direction.

## Layers

```
┌────────────────────────────────────────────────────────────────────┐
│  4. Plugins   (plugins/)                                          │
│     AI providers (Copilot, Claude, Codex, Ollama) + future exts   │
├────────────────────────────────────────────────────────────────────┤
│  3. UI + Features   (src/)                                        │
│     MainWindow, docks, panels — depends only on Core interfaces   │
├────────────────────────────────────────────────────────────────────┤
│  2. Platform bindings   (src/core/qt*)                            │
│     Concrete Qt implementations — replaceable                     │
├────────────────────────────────────────────────────────────────────┤
│  1. Core interfaces   (src/core/i*)                               │
│     IFileSystem, IProcess, ITerminal, INetwork — no UI deps       │
└────────────────────────────────────────────────────────────────────┘
```

Dependencies flow **downward only**. Plugins depend on interfaces, never on
concrete core classes.

## Core Subsystems (all in `src/`)

| Subsystem | Directory | Purpose | Key interfaces / types |
|-----------|-----------|---------|----------------------|
| **App shell** | `mainwindow.*`, `main.cpp` | Window, tabs, docks, menus, status bar | `MainWindow` |
| **Editor** | `editor/` | Text editing, syntax highlighting, piece table, minimap | `EditorView`, `SyntaxHighlighter`, `PieceTableBuffer` |
| **Language Intelligence** | `lsp/` | LSP client, Clangd lifecycle, completion, hover, diagnostics | `LspClient`, `ClangdManager`, `LspEditorBridge` |
| **Terminal** | `terminal/` | ConPTY/PTY emulator, VT100/xterm, scrollback | `TerminalView`, `TerminalPanel` |
| **Build System** | `build/` | Task runner, build profiles, problem matchers | `OutputPanel`, `RunPanel`, `TaskProfile` |
| **Source Control** | `git/` | Git status, blame, diff, staging, branches | `GitService`, `GitPanel` |
| **Search** | `search/` | Workspace search, file search, regex search | `SearchPanel`, `WorkspaceIndexer` |
| **Project** | `project/` | Solution/project tree, workspace management | `ProjectManager`, `SolutionModel` |
| **MCP** | `mcp/` | Model Context Protocol client for tool servers | `McpManager`, `McpConnection` |
| **Debug** | `debug/` | Debug adapter framework, GDB/MI, breakpoints | `IDebugAdapter`, `GdbMiAdapter`, `DebugPanel` |
| **Remote / SSH** | `remote/` | SSH connections, remote FS, multi-arch probing, sync | `SshSession`, `SshConnectionManager`, `RemoteFilePanel`, `RemoteHostProber`, `RemoteSyncService` |
| **Agent framework** | `agent/` | Agent runtime, tools, chat UI, session management | `AgentController`, `AgentOrchestrator`, `ChatPanelWidget` |
| **Project Brain** | `agent/projectbrain*` | Persistent workspace knowledge (rules, facts, sessions) | `ProjectBrainService`, `BrainContextBuilder`, `MemorySuggestionEngine` |
| **Core abstractions** | `core/` | OS interfaces (filesystem, process, terminal, network) | `IFileSystem`, `IProcess`, `ITerminal`, `INetwork` |
| **Plugin system** | `pluginmanager.*`, `serviceregistry.*` | Plugin loader, typed service resolution | `PluginManager`, `ServiceRegistry` |
| **UI framework** | `ui/`, `commandpalette.*`, `thememanager.*` | Command palette, theme engine, keymap, notifications, custom docking | `CommandPalette`, `ThemeManager`, `KeymapManager`, `NotificationToast`, `DockManager`, `ExDockWidget` |
| **Logger** | `logger.*` | Thread-safe timestamped logging | `Logger` |

## Core interfaces (`src/core/`)

| Interface | Implementation | Purpose |
|-----------|---------------|---------|
| `IFileSystem` | `QtFileSystem` | File read/write/exist/list |
| `IProcess` | `QtProcess` | Process launch and I/O |
| `ITerminal` | `QtTerminal` | Terminal emulator backend |
| `INetwork` | `NullNetwork` | HTTP requests (stub) |

## Service Registry

All core subsystems register services in `ServiceRegistry` by string key.
Plugins resolve services at runtime — never via direct `#include` of core classes.

```cpp
// Registration (core)
registry->registerService("projectBrainService", m_brain);

// Resolution (plugin or other subsystem)
auto *brain = registry->service<ProjectBrainService>("projectBrainService");
```

## Plugin Architecture

Plugins implement `IPlugin` (from `plugininterface.h`) and optionally `IAgentPlugin`
(from `agent/iagentplugin.h`). Each plugin resides in its own directory under `plugins/`
with a standalone `CMakeLists.txt`.

| Plugin | Directory | Purpose |
|--------|-----------|---------|
| Copilot | `plugins/copilot/` | GitHub Copilot AI provider |
| Claude | `plugins/claude/` | Anthropic Claude AI provider |
| Codex | `plugins/codex/` | OpenAI Codex AI provider |

Plugin extension interfaces (defined in `src/agent/`):

| Interface | File | Purpose |
|-----------|------|---------|
| `IAgentSettingsPageProvider` | `iagentsettingspageprovider.h` | Plugin-provided settings UI |
| `IChatSessionImporter` | `ichatsessionimporter.h` | Vendor transcript import |
| `IProviderAuthIntegration` | `iproviderauthintegration.h` | Unified auth lifecycle |

## Custom Docking System (`src/ui/dock/`)

Replaces Qt's built-in `QDockWidget` with a fully custom VS Code–style docking
framework. The system lives in the `exdock` namespace.

### Layout

```
Root (horizontal DockSplitter)
├── Left DockArea(s)     — tabbed panels (Project, Search, Git, Outline, Remote)
├── Center (vertical DockSplitter)
│   ├── Top DockArea(s)
│   ├── Editor widget    — breadcrumb bar + tab widget
│   └── Bottom DockArea(s) — tabbed panels (Terminal, References, Output, Run, etc.)
└── Right DockArea(s)    — tabbed panels (AI, Memory, MCP)
```

### Key Types

| Type | File | Purpose |
|------|------|---------|
| `DockManager` | `DockManager.h` | Top-level orchestrator; creates splitters, sidebars, overlay; manages pin/unpin/close/toggle lifecycle; JSON state save/restore |
| `ExDockWidget` | `ExDockWidget.h` | QWidget-based replacement for QDockWidget; holds a content widget, dock ID, preferred size, state |
| `DockArea` | `DockArea.h` | Tabbed container for multiple ExDockWidgets; has DockTabBar + DockTitleBar + QStackedWidget |
| `DockSplitter` | `DockSplitter.h` | Recursive QSplitter with stretch factors, minimum sizes, ratio save/restore |
| `DockOverlayPanel` | `DockOverlayPanel.h` | Slide-out overlay for auto-hidden docks; pin/close buttons; auto-hides on outside click |
| `DockSideBar` | `DockSideBar.h` | Edge sidebar with DockSideTab buttons for auto-hidden docks |
| `DockSideTab` | `DockSideTab.h` | Individual sidebar tab button |
| `DockTabBar` | `DockTabBar.h` | Tab bar within DockArea; supports drag reordering |
| `DockTitleBar` | `DockTitleBar.h` | Title bar with pin/close/float action buttons |
| `DockTypes.h` | `DockTypes.h` | Enums: `DockState` (Closed, Docked, AutoHidden, Floating), `SideBarArea` (Left, Right, Top, Bottom, None) |

### Dock States

- **Docked**: Visible in a DockArea within the splitter layout.
- **AutoHidden**: Collapsed to a sidebar tab; slide-out overlay on click.
- **Closed**: Not visible, no sidebar tab; can be re-shown via View menu.
- **Floating**: (Future) Detached window.

## Rules
- UI must not call OS APIs directly.
- Only Core implementations may touch Qt/OS facilities.
- Plugins never include internal headers.
- Dependencies flow downward only: Plugins → UI → Core.
- All background work is async/cancelable — typing never blocks.

## Target Outcome
Any platform port should require only replacing Core bindings without touching
UI or plugin layers.
