# Architecture

The goal is strict modularity, portability, and minimal footprint. The code is
organized into layers with clear dependency direction.

## Layers

```
┌────────────────────────────────────────────────────────────────────┐
│  5. Plugins   (plugins/)                                          │
│     AI providers (Copilot, Claude, Codex, Ollama) + future exts   │
├────────────────────────────────────────────────────────────────────┤
│  4. SDK — Stable Plugin API   (src/sdk/)                          │
│     IHostServices, ICommandService, IViewService, IEditorService  │
│     IWorkspaceService, IGitService, ITerminalService, etc.        │
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

Dependencies flow **downward only**. Plugins depend on SDK interfaces, never on
concrete core classes or MainWindow.

## Core Subsystems (all in `src/`)

| Subsystem | Directory | Purpose | Key interfaces / types |
|-----------|-----------|---------|----------------------|
| **App shell** | `mainwindow.*`, `main.cpp` | Window, tabs, docks, menus, status bar | `MainWindow` |
| **Editor** | `editor/` | Text editing, syntax highlighting, piece table, minimap | `EditorView`, `SyntaxHighlighter`, `PieceTableBuffer` |
| **Language Intelligence** | `lsp/` | LSP client, Clangd lifecycle, completion, hover, diagnostics | `LspClient`, `ClangdManager`, `LspEditorBridge` |
| **Terminal** | `terminal/` | ConPTY/PTY emulator, VT100/xterm, scrollback | `TerminalView`, `TerminalPanel` |
| **Build System** | `build/` | Task runner, build profiles, problem matchers, toolchain detection, CMake integration, build toolbar, debug launch | `OutputPanel`, `RunPanel`, `TaskProfile`, `ToolchainManager`, `CMakeIntegration`, `BuildToolbar`, `DebugLaunchController` |
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
| **SDK** | `sdk/` | Stable plugin API — typed host services, permissions | `IHostServices`, `HostServices`, `PluginPermission` |
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

## Plugin SDK (`src/sdk/`)

The SDK is the **stable API boundary** between the IDE core and plugins.
Plugins receive a single `IHostServices*` on initialization and use typed
service interfaces to interact with the IDE. No plugin should ever reference
`MainWindow`, internal panels, or concrete service implementations.

### Entry Point

```cpp
bool MyPlugin::initialize(IHostServices *host) override
{
    host->commands()->registerCommand("myPlugin.run", "Run", [host]() {
        host->notifications()->info("Running...");
    });
    return true;
}
```

### SDK Services

| Interface | File | Purpose |
|-----------|------|---------|
| `IHostServices` | `ihostservices.h` | Root — provides access to all services |
| `ICommandService` | `icommandservice.h` | Register/execute commands |
| `IWorkspaceService` | `iworkspaceservice.h` | File system, open files, workspace root |
| `IEditorService` | `ieditorservice.h` | Active document, cursor, selection |
| `IViewService` | `iviewservice.h` | Register/show/hide plugin panels |
| `INotificationService` | `inotificationservice.h` | Info/warning/error messages |
| `IGitService` | `igitservice.h` | Branch, diff, changed files |
| `ITerminalService` | `iterminalservice.h` | Run commands, read output |
| `IDiagnosticsService` | `idiagnosticsservice.h` | LSP diagnostics |
| `ITaskService` | `itaskservice.h` | Build/test task runner |

### Permissions

Plugins declare required permissions in `PluginInfo::requestedPermissions`.
The IDE grants only what the user approves.

| Permission | Grants |
|------------|--------|
| `FilesystemRead` | Read files in workspace |
| `FilesystemWrite` | Create/modify/delete files |
| `TerminalExecute` | Spawn processes |
| `GitRead` | Branch, status, blame, diff |
| `GitWrite` | Stage, commit, push |
| `NetworkAccess` | Outbound HTTP/WebSocket |
| `DiagnosticsRead` | Read LSP diagnostics |
| `WorkspaceRead` | Open files, project structure |
| `WorkspaceWrite` | Modify workspace settings |
| `AgentToolInvoke` | Invoke agent tools |

### Concrete Implementation

`HostServices` (in `hostservices.h/cpp`) bridges SDK interfaces to real subsystems.
It is created by `MainWindow` and passed to `PluginManager::initializeAll(IHostServices*)`.
The concrete implementations (`CommandServiceImpl`, `WorkspaceServiceImpl`, etc.) are
internal to the core and never exposed to plugins.

### C ABI (`src/sdk/cabi/`)

The C ABI layer provides a **compiler-independent, stable binary interface** for
plugins built without Qt or the IDE's C++ ABI. Any language with C FFI support
(Rust, Go, Zig, C, C++ with any compiler) can build Exorcist plugins.

| File | Purpose |
|------|---------|
| `exorcist_plugin_api.h` | **THE** stable C header — versioned ABI, all types, host API vtable, plugin entry points |
| `exorcist_sdk.hpp` | C++ header-only wrapper (RAII strings, service wrappers) — no Qt dependency |
| `cabi_bridge.h/cpp` | Host-side bridge: `CAbiPluginBridge` populates ExHostAPI vtable with static callbacks that delegate to IHostServices; `CAbiProviderAdapter` wraps C vtable AI providers as `IAgentProvider` QObjects |

**Plugin entry points** (plugin exports via `extern "C"`):
- `ex_plugin_api_version()` — returns `EX_ABI_VERSION` for compatibility check
- `ex_plugin_describe()` — returns `ExPluginDescriptor` with plugin metadata
- `ex_plugin_initialize(const ExHostAPI *api)` — receives host services vtable
- `ex_plugin_shutdown()` — cleanup

**Host API vtable** (`ExHostAPI`): ~40 function pointers covering editor, workspace,
notifications, commands, git, terminal, diagnostics, AI provider registration,
AI response callbacks, and logging. All strings are UTF-8 `const char*`; host-allocated
strings are freed via `api->free_string()`.

**AI providers via C ABI**: Plugins fill an `ExAIProviderVTable` and register it
during `ex_plugin_initialize()` via `api->ai_register_provider()`. The host wraps
it as a Qt `IAgentProvider` through `CAbiProviderAdapter`.

**Loading**: `PluginManager` tries `QPluginLoader` first (Qt C++ plugins). On failure,
it resolves `ex_plugin_*` symbols via `QLibrary` and creates a `CAbiPluginBridge`.

## Plugin Architecture

Plugins implement `IPlugin` (from `plugininterface.h`) and optionally `IAgentPlugin`
(from `agent/iagentplugin.h`). Each plugin resides in its own directory under `plugins/`
with a standalone `CMakeLists.txt`.

**Qt C++ plugins** use `QPluginLoader` and share the IDE's C++ ABI. They must be
compiled with the same compiler and Qt version as the host.

**C ABI plugins** export `extern "C"` entry points and can be built with any
compiler or language. They interact with the IDE through the `ExHostAPI` vtable
passed during initialization. See `src/sdk/cabi/exorcist_plugin_api.h`.

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
