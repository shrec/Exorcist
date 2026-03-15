# Architecture

The goal is strict modularity, portability, and minimal footprint. The code is
organized into layers with clear dependency direction.

See [core-philosophy.md](core-philosophy.md) for the foundational design philosophy
(Core IDE vs Core Plugins separation, activation model, performance principles).

## Layers

```
┌────────────────────────────────────────────────────────────────────┐
│  6. Plugins   (plugins/)                                          │
│     AI providers (Copilot, Claude, Codex, Ollama) + future exts   │
├────────────────────────────────────────────────────────────────────┤
│  5. Components   (components/)                                    │
│     DLL-based UI panels: terminal, file tree, UART, etc.          │
├────────────────────────────────────────────────────────────────────┤
│  4. SDK — Stable Plugin API   (src/sdk/)                          │
│     IHostServices, ICommandService, IViewService, IEditorService  │
│     IWorkspaceService, IGitService, IComponentService, etc.       │
├────────────────────────────────────────────────────────────────────┤
│  3. UI + Features   (src/)                                        │
│     MainWindow, docks, panels — depends only on Core interfaces   │
├────────────────────────────────────────────────────────────────────┤
│  2. Platform bindings   (src/core/qt*)                            │
│     Concrete Qt implementations — replaceable                     │
├────────────────────────────────────────────────────────────────────┤
│  1. Core interfaces   (src/core/i*)                               │
│     IFileSystem, IFileWatcher, IEnvironment, IProcess, ITerminal  │
│     IComponentFactory                                             │
└────────────────────────────────────────────────────────────────────┘
```

Dependencies flow **downward only**. Plugins depend on SDK interfaces, never on
concrete core classes or MainWindow.

## Core Subsystems (all in `src/`)

| Subsystem | Directory | Purpose | Key interfaces / types |
|-----------|-----------|---------|----------------------|
| **App shell** | `mainwindow.*`, `main.cpp` | Window, tabs, docks, menus, status bar | `MainWindow` |
| **Editor** | `editor/` | Text editing, syntax highlighting (tree-sitter + regex fallback), piece table, minimap, vim emulation. `HighlighterFactory` is registry-aware: consults `ContributionRegistry` language declarations from plugins (via `setLanguageLookup` callback) before falling back to hard-coded extension maps. `EditorManager` centralises tab/project/tree state previously scattered across `MainWindow` (Phase D2). | `EditorView`, `EditorManager`, `HighlighterFactory`, `TreeSitterHighlighter`, `SyntaxHighlighter`, `PieceTableBuffer`, `VimHandler` |
| **Language Intelligence** | `lsp/` + `plugins/cpp-language/` | LSP client, completion, hover, diagnostics, TCP socket transport (Core); Clangd lifecycle (**Core Plugin** in `plugins/cpp-language/`). ILspService SDK interface in `src/sdk/`. | `LspClient`, `LspEditorBridge`, `SocketLspTransport` (Core); `CppLanguagePlugin`, `ClangdManager`, `LspServiceBridge` (Plugin) |
| **Terminal** | `terminal/` | ConPTY/PTY emulator, VT100/xterm, scrollback | `TerminalView`, `TerminalPanel` |
| **Build System** | `plugins/build/` | **Core Plugin** — CMake integration, toolchain detection, build toolbar, debug launch controller, output panel, run panel. Extracted from `src/build/` in Phase C. OutputPanel and RunLaunchPanel remain in `src/build/` as shared UI types. | `BuildPlugin`, `CMakeIntegration`, `ToolchainManager`, `BuildToolbar`, `DebugLaunchController`, `BuildSystemService` |
| **Testing** | `plugins/testing/` | **Core Plugin** — Test discovery (CTest JSON), test runner, test explorer UI. Extracted from `src/testing/` in Phase C. Contextual activation on CTestTestfile.cmake/CMakeLists.txt detection. | `TestingPlugin`, `TestDiscoveryService`, `TestExplorerPanel`, `TestRunnerService` |
| **Problems** | `problems/` | Unified diagnostics panel aggregating LSP + build errors | `ProblemsPanel` |
| **Source Control** | `git/` | Git status, blame, diff, staging, branches, multi-file diff explorer, 3-way merge editor | `GitService`, `GitPanel`, `DiffExplorerPanel`, `MergeEditor` |
| **Search** | `search/` | Workspace search, file search, regex search | `SearchPanel`, `WorkspaceIndexer` |
| **Code Graph** | `codegraph/` | SQLite-based code intelligence engine — indexes files, classes, functions (with exact line ranges via brace-depth tracking), edges, Qt connections, services, test cases, CMake targets. 18-table schema + FTS5 search. Language-agnostic via `ILanguageIndexer` interface: built-in indexers for C++, TypeScript/JS, Python; new languages added by implementing the interface. See [docs/codegraph.md](codegraph.md). | `CodeGraphDb`, `CodeGraphIndexer`, `CodeGraphQuery`, `ILanguageIndexer`, `CppLanguageIndexer`, `TsLanguageIndexer`, `PythonLanguageIndexer` |
| **Project** | `project/` | Solution/project tree, workspace management | `ProjectManager`, `SolutionModel` |
| **MCP** | `mcp/` | Model Context Protocol client for tool servers | `McpManager`, `McpConnection` |
| **Debug** | `plugins/debug/` | **Core Plugin** — GDB/MI debug adapter, debug panel with call stack/locals/breakpoints/watch/output tabs, watch tree model, quick watch dialog. Extracted from `src/debug/` in Phase C. IDebugAdapter and IDebugService SDK interfaces in `src/sdk/`. | `DebugPlugin`, `GdbMiAdapter`, `DebugPanel`, `WatchTreeModel`, `QuickWatchDialog`, `DebugServiceBridge` |
| **Agent framework** | `agent/` | Agent runtime, tools, chat UI (Qt widgets or Ultralight HTML renderer), session management, diagnostics push, tree-sitter AST access, agent UI protocol (structured event bus + live dashboard) | `AgentController`, `AgentOrchestrator`, `ChatPanelWidget`, `UltralightWidget`, `ChatJSBridge`, `DiagnosticsNotifier`, `TreeSitterAgentHelper`, `AgentUIBus`, `AgentDashboardPanel`, `DashboardJSBridge` |
| **ExoBridge** | `process/`, `server/` | IPC protocol, shared daemon, cross-instance process management | `ExoBridgeCore`, `BridgeClient`, `ProcessManager`, `Ipc::Message` |
| **Project Brain** | `agent/projectbrain*`, `agent/tools/managerules*`, `agent/tools/managememory*` | Persistent workspace knowledge (rules, facts, notes, sessions). Provides `manage_rules` and `manage_memory` tools so the AI model can read/write project rules and memory facts. Data stored in `.exorcist/` (rules.json, memory.json, notes/). `BrainContextBuilder` auto-injects rules, facts, and notes into system prompts. | `ProjectBrainService`, `BrainContextBuilder`, `MemorySuggestionEngine`, `ManageRulesTool`, `ManageMemoryTool` |
| **Core abstractions** | `core/` | OS interfaces (filesystem, file watching, environment, process, terminal, network) + UI manager interfaces (dock panels, menus, toolbars, status bar, workspace). These interfaces form the plugin-first extension model: the core IDE is a minimal shell, and everything else is contributed by plugins through these interfaces at runtime. | `IFileSystem`, `IFileWatcher`, `IEnvironment`, `IProcess`, `ITerminal`, `INetwork`, `IDockManager`, `IMenuManager`, `IToolBarManager`, `IStatusBarManager`, `IWorkspaceManager` |
| **Plugin system** | `pluginmanager.*`, `serviceregistry.*`, `plugin/` | Plugin loader (Qt + C ABI + Lua + JS), typed service resolution, extension gallery with enable/disable toggle, marketplace (registry, download, install). Plugins declare `plugin.json` manifests with activation events and contributions. Disabled plugins persisted in QSettings and skipped during initialization. | `PluginManager`, `ServiceRegistry`, `PluginGalleryPanel`, `PluginMarketplaceService`, `PluginManifest` |
| **JavaScript SDK** | `plugins/javascript-sdk/` | **Core Plugin** — Ultralight JSC-based runtime for JavaScript plugins. Two plugin types: **headless** (pure JS via `JSGlobalContext`, no DOM) and **HTML** (full WebView via `UltralightPluginView` with DOM + CSS + `ex.*` SDK). Loads JS plugins from `plugins/javascript/`. Each plugin gets sandboxed context with permission-gated `ex.*` host API. HTML plugins declare `contributions.views` in their manifest and are registered as dock panels via `IViewService`. Supports command registration, editor/workspace/git/diagnostics read, notifications, logging, events, and hot reload. | `JsPluginSdkPlugin`, `JsPluginRuntime`, `JsHostAPI`, `UltralightPluginView`, `LoadedHtmlPlugin` |
| **Bootstrap** | `bootstrap/` | Subsystem bootstrappers that own and wire groups of related objects, reducing MainWindow init code. Includes concrete adapters that bridge core UI interfaces to real implementations: `DockManagerAdapter` (wraps `DockManager` behind `IDockManager`), `MenuManagerImpl` (manages `QMenuBar` behind `IMenuManager`), `ToolBarManagerAdapter` (wraps `DockToolBarManager` behind `IToolBarManager`), `StatusBarManagerAdapter` (wraps `QStatusBar` behind `IStatusBarManager`), `WorkspaceManagerImpl` (wraps workspace ops behind `IWorkspaceManager`). | `BridgeBootstrap`, `StatusBarManager`, `AIServicesBootstrap`, `DockManagerAdapter`, `MenuManagerImpl`, `ToolBarManagerAdapter`, `StatusBarManagerAdapter`, `WorkspaceManagerImpl` |
| **Settings** | `settings/` | Hierarchical settings: global QSettings → workspace `.exorcist/settings.json` override layer | `WorkspaceSettings` |
| **SDK** | `sdk/` | Stable plugin API — typed host services, permissions, service registration, build/launch/debug/LSP/UI-manager/component/profile interfaces. `IHostServices` exposes `docks()`, `menus()`, `toolbars()`, `statusBar()`, `workspaceManager()`, `components()`, `profiles()` so plugins can contribute panels, menus, toolbar items, status bar entries, create component instances, and query profiles at runtime. | `IHostServices`, `HostServices`, `PluginPermission`, `IBuildSystem`, `ILaunchService`, `IDebugAdapter`, `IDebugService`, `ILspService`, `IComponentService`, `IProfileManager` |
| **UI framework** | `ui/`, `commandpalette.*`, `thememanager.*` | Command palette, theme engine with token colors, keymap, notifications, custom docking, theme gallery | `CommandPalette`, `ThemeManager`, `KeymapManager`, `NotificationToast`, `DockManager`, `ExDockWidget`, `ThemeGalleryPanel` |
| **Logger** | `logger.*` | Thread-safe timestamped logging | `Logger` |
| **Crash Handler** | `crashhandler.*` | Catches unhandled exceptions/signals; writes minidumps (Windows), CPU register dumps, faulting module identification, access violation details, stack traces via StackWalk64, loaded modules list, and recent log lines ring buffer to `crashes/` directory. Integrated with Logger for automatic log capture. | `CrashHandler` |
| **Components** | `component/` + `components/` | Dynamic component DLL system — loads shared libraries at runtime that provide UI panels. Supports multiple instances per component type. | `ComponentRegistry`, `ComponentServiceAdapter`, `IComponentFactory` |
| **Profiles** | `profile/` + `profiles/` | Development profile system — workspace-aware plugin activation, detection rules (file existence, directory existence, file content regex), dock/settings presets. JSON-based profile manifests with auto-detection scoring. | `ProfileManager`, `ProfileManifest`, `IProfileManager` |
| **Performance** | `startupprofiler.*`, `cmake/report_binary_size.cmake` | Startup phase timing, RSS measurement, budget enforcement (300 ms / 80 MB), post-build binary size reporting | `StartupProfiler` |

## Core interfaces (`src/core/`)

| Interface | Implementation | Purpose |
|-----------|---------------|---------|
| `IFileSystem` | `QtFileSystem` | File read/write/exist/list |
| `IFileWatcher` | `QtFileWatcher` | File/directory change monitoring |
| `IEnvironment` | `QtEnvironment` | Environment variables, platform detection |
| `IProcess` | `QtProcess` | Process launch and I/O |
| `ITerminal` | `QtTerminal` | Terminal emulator backend |
| `INetwork` | `NullNetwork` | HTTP requests (stub) |
| `IDockManager` | `DockManagerAdapter` | Dock panel lifecycle (add, show, hide, pin, unpin) |
| `IMenuManager` | `MenuManagerImpl` | Menu bar contribution (standard + custom menus, actions) |
| `IToolBarManager` | `ToolBarManagerAdapter` | Toolbar creation and management |
| `IStatusBarManager` | `StatusBarManagerAdapter` | Status bar items and messages |
| `IWorkspaceManager` | `WorkspaceManagerImpl` | Workspace/folder/file operations |
| `IComponentFactory` | (per-DLL) | Component DLL factory — each DLL provides one panel type |
| `IProfileManager` | `ProfileManager` | Development profiles — activation, detection, switching |

## Component DLL System (`components/`)

Components are lightweight shared libraries (`.dll` / `.so` / `.dylib`) that
each provide exactly one type of UI panel. Unlike plugins (which have lifecycle,
permissions, manifests), components are simple widget factories:

| Concept | Description |
|---------|-------------|
| `IComponentFactory` | Interface implemented by each component DLL — metadata + `createInstance()` |
| `ComponentRegistry` | Loads component DLLs, manages factories and instance lifecycle |
| `IComponentService` | SDK interface for plugins — create/destroy/query component instances |
| `ComponentServiceAdapter` | Bridges `ComponentRegistry` to `IComponentService` |

Components support **multiple instances** (e.g., 3 terminals, 2 UART monitors
for different COM ports). Each instance gets wrapped in `ExDockWidget` and
registered with `DockManager` automatically.

### Component Factory Contract

```cpp
class IComponentFactory {
    virtual QString componentId() const = 0;        // "exorcist.terminal"
    virtual QString displayName() const = 0;        // "Terminal"
    virtual bool supportsMultipleInstances() const = 0;
    virtual DockArea preferredArea() const = 0;      // Left/Right/Bottom/Center
    virtual QWidget *createInstance(const QString &instanceId,
                                    IHostServices *hostServices,
                                    QWidget *parent) = 0;
};
```

### Current Components

| Component | DLL | Multi-instance | Category |
|-----------|-----|---------------|----------|
| Terminal | `libterminal.dll` | Yes | Core |

## Profile System (`profile/` + `profiles/`)

Development profiles let the IDE adapt its plugin set, dock layout, and settings
based on the workspace type (C++, Rust, Python, etc.). Profile manifests are
JSON files in `profiles/` loaded at startup.

| Concept | Description |
|---------|-------------|
| `IProfileManager` | Core interface — activate/deactivate profiles, detect workspace type |
| `ProfileManager` | Concrete implementation — loads JSON manifests, scores workspaces, manages plugin sets |
| `ProfileManifest` | Data class — id, name, detection rules, required/disabled plugins, dock defaults, settings |
| `ProfileDetectionRule` | Workspace scoring rule — `fileExists`, `directoryExists`, `fileContent` with weight |

### Detection

Each profile declares detection rules with weights. The workspace is scored
against all registered profiles; the highest score above the threshold wins.

### Profile Manifest

```json
{
  "id": "cpp-native",
  "name": "C++ Native",
  "detection": [
    { "type": "fileExists", "pattern": "CMakeLists.txt", "weight": 3 }
  ],
  "detectionThreshold": 2,
  "requiredPlugins": ["copilot"],
  "disabledPlugins": [],
  "dockDefaults": [
    { "id": "TerminalDock", "visible": true }
  ]
}
```

### Bundled Profiles

| Profile | Detection | Key Plugins |
|---------|-----------|-------------|
| C++ Native | `CMakeLists.txt` / `*.vcxproj` | copilot, clangd |
| Rust | `Cargo.toml` | copilot |
| Python | `requirements.txt` / `pyproject.toml` | copilot |
| Web/JS | `package.json` | copilot |
| Go | `go.mod` | copilot |
| General | Default fallback | copilot |
| Minimal | Explicit only | (none) |

## Service Registry

All core subsystems register services in `ServiceRegistry` by string key.
Plugins resolve services at runtime — never via direct `#include` of core classes.

Services can be registered with version metadata (`ServiceContract`) for
compatibility checking at resolution time:

```cpp
// Registration with version contract
registry->registerService("projectBrainService", m_brain,
                           ServiceContract{1, 2, "Project brain service"});

// Typed resolution
auto *brain = registry->service<ProjectBrainService>("projectBrainService");

// Version compatibility check (major must match, minor >= required)
if (registry->isCompatible("projectBrainService", 1, 2)) { /* safe to use */ }
```

Legacy `registerService(name, service)` without a contract defaults to v1.0.

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
| `IBuildSystem` | `ibuildsystem.h` | Build/configure/clean, targets, build directory |
| `ITestRunner` | `itestrunner.h` | Test discovery, execution, results |
| `ContributionRegistry` | `contributionregistry.h` | Wires manifest contributions to IDE subsystems |
| `IDockManager` | `core/idockmanager.h` | Add/remove/show/hide/pin dock panels |
| `IMenuManager` | `core/imenumanager.h` | Contribute menu items and context actions |
| `IToolBarManager` | `core/itoolbarmanager.h` | Create toolbars, add actions and widgets |
| `IStatusBarManager` | `core/istatusbarmanager.h` | Add/remove status bar items |
| `IWorkspaceManager` | `core/iworkspacemanager.h` | Open folders/files, recent items, workspace state |

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

### LuaJIT Scripting (`src/sdk/luajit/`)

The LuaJIT layer provides **sandboxed Lua scripting** for lightweight automation
plugins. Scripts cannot access FFI, io, os, or write to the editor — they are
restricted to read-only operations and command registration.

| File | Purpose |
|------|---------|
| `luascriptengine.h/cpp` | Engine: sandbox, memory limiter, hot reload, event dispatch |
| `luahostapi.h/cpp` | Binds `IHostServices` into Lua as `ex.*` table hierarchy |

**Key safety features:**
- Per-plugin `lua_State` (no shared globals)
- Custom allocator with per-plugin memory cap (default 16 MB)
- Instruction count hook (10M limit) prevents infinite loops
- `ffi`, `io`, `os`, `debug`, `package`, `require` all blocked
- Permission-gated API access from `plugin.json`
- Hot reload via `QFileSystemWatcher` with 300ms debounce
- Traceback error handler on all `pcall` invocations

**Lua API surface** (`ex.*`): commands, editor (read), notifications, workspace (read),
git (read), diagnostics (read), logging, events, runtime/memory query.

**Loading**: `PluginManager::loadLuaPluginsFrom()` scans `plugins/lua/*/` for
subdirectories containing `plugin.json` + `main.lua`.

See [docs/luajit.md](luajit.md) for full API reference and plugin authoring guide.

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
| GitHub | `plugins/github/` | GitHub CLI integration — PRs, Issues, Actions, Releases |
| Remote / SSH | `plugins/remote/` | SSH connections, remote file browsing, sync |
| Copilot | `plugins/copilot/` | GitHub Copilot AI provider |
| Claude | `plugins/claude/` | Anthropic Claude AI provider |
| Codex | `plugins/codex/` | OpenAI Codex AI provider |
| Hello World | `plugins/lua/hello-world/` | Sample Lua plugin (commands, events) |
| Word Count | `plugins/lua/word-count/` | Sample Lua plugin (text analysis) |

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
├── Left DockArea(s)     — tabbed panels (Project, Search, Git, Outline)
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

## ExoBridge (`server/` + `src/process/`)

A persistent daemon process (`exobridge`) that manages workspace-agnostic
resources shared across all Exorcist IDE instances on the same machine.
Connects via `QLocalSocket` (named pipe on Windows, Unix domain socket elsewhere).

```
┌──────────────────┐   QLocalSocket   ┌──────────────────────────┐
│  Exorcist IDE 1  │─────────────────▶│                          │
│  (BridgeClient)  │                  │   exobridge              │
├──────────────────┤                  │   (ExoBridgeCore)        │
│  Exorcist IDE 2  │─────────────────▶│                          │
│  (BridgeClient)  │                  │  • MCP tool servers      │
└──────────────────┘                  │  • Git file watcher      │
                                      │  • Auth token cache      │
                                      └──────────────────────────┘
```

### What belongs in ExoBridge

| Resource | Why shared |
|----------|-----------|
| **MCP tool servers** | Independent child processes, stateless tool calls |
| **Git file watcher** | Per-repo, not per-window; one watcher per repo |
| **Auth token cache** | OAuth tokens, API keys — one login benefits all instances |

### What stays per-IDE-instance

| Resource | Why per-instance |
|----------|-----------------|
| **LSP / Clangd** | Per-session protocol, per-file state, per-cursor context |
| **Workspace file index** | Per-workspace/solution — different projects have different trees |
| **Search** | Per-workspace scope, tied to the open solution |
| **Terminal** | Per-window PTY, interactive I/O |
| **Build / Debug** | Per-task, per-target, output tied to one window |

### Lifecycle

1. First IDE instance starts → `BridgeClient` tries to connect.
2. No daemon running → `BridgeClient` launches `exobridge` via `QProcess::startDetached()`.
3. Daemon creates a `QLockFile` (PID lock) and listens on the named pipe.
4. IDE connects, sends `Handshake`.
5. Additional IDE instances connect to the same daemon.
6. When all IDEs close, the daemon stays running (persistent mode, default).
7. `--idle-timeout <seconds>` flag enables auto-shutdown after N seconds with no clients.

### Wire Protocol

JSON-RPC 2.0 over length-prefixed frames:

| Layer | Format |
|-------|--------|
| **Framing** | 4-byte big-endian length + UTF-8 JSON payload |
| **Requests** | `{ "jsonrpc": "2.0", "id": N, "method": "...", "params": {...} }` |
| **Responses** | `{ "jsonrpc": "2.0", "id": N, "result": {...} }` or `"error"` |
| **Notifications** | `{ "jsonrpc": "2.0", "method": "...", "params": {...} }` (no id) |

### Key Types

| Type | File | Purpose |
|------|------|---------|
| `Ipc::Message` | `process/ipcprotocol.h` | Wire message — serialize/deserialize, frame/unframe |
| `ExoBridgeCore` | `process/exobridgecore.h` | Daemon core — QLocalServer, client tracking, service handler dispatch |
| `BridgeClient` | `process/bridgeclient.h` | IDE-side IPC client — auto-launch, reconnect, service calls |
| `McpBridgeService` | `server/mcpbridgeservice.h` | Centralized MCP server process management (start/stop/callTool) |
| `GitWatchBridgeService` | `server/gitwatchservice.h` | Centralized git file watching — one watcher per repo |
| `AuthTokenBridgeService` | `server/authtokenbridgeservice.h` | Centralized auth token cache — DPAPI/Keychain/libsecret |
| `ProcessManager` | `process/processmanager.h` | Unified API for local (per-window) and shared (daemon) processes |
| `BridgeBootstrap` | `bootstrap/bridgebootstrap.h` | Wires BridgeClient + ProcessManager into ServiceRegistry |

### IPC Methods

| Method | Direction | Purpose |
|--------|-----------|---------|
| `handshake` | IDE → ExoBridge | Register instance, receive daemon info |
| `listServices` | IDE → ExoBridge | Enumerate registered shared services |
| `registerService` | IDE → ExoBridge | Publish a service to other instances |
| `callService` | IDE → ExoBridge | Invoke a shared service method |
| `processStart/Kill/List` | IDE → ExoBridge | Manage daemon-side child processes |
| `shutdown` | IDE → ExoBridge | Request graceful daemon shutdown |
| `serviceEvent` | ExoBridge → IDE | Broadcast service registration changes |
| `serverShutdown` | ExoBridge → IDE | Notify all clients before exit |

## Rules
- UI must not call OS APIs directly.
- Only Core implementations may touch Qt/OS facilities.
- Plugins never include internal headers.
- Dependencies flow downward only: Plugins → UI → Core.
- All background work is async/cancelable — typing never blocks.

## Target Outcome
Any platform port should require only replacing Core bindings without touching
UI or plugin layers.
