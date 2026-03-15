# Plugin API (Draft)

This document defines the minimal interface for Exorcist plugins. The goal is
to keep the contract stable and small so contributors can build plugins with
low friction.

## Goals
- Simple, explicit lifecycle (initialize/shutdown).
- Minimal required metadata (id, name, version).
- No hard dependency on internal UI classes.
- Provider plugins should integrate with shared platform services, not reimplement chat runtime or tools.

## Interface
Plugins implement `IPlugin` and export it via Qt's plugin system.
`initialize()` receives a `ServiceRegistry` pointer as `QObject *services`.

```
struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual void initialize(QObject *services) = 0;
    virtual void shutdown() = 0;
};
```

## Loading
- Exorcist loads plugins from `./plugins` next to the executable.
- Each plugin is a shared library (`.dll`, `.so`, `.dylib`).
- Invalid plugins are skipped and logged.

## Minimal Example (Pseudo)
```
class MyPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.exorcist.IPlugin/1.0")
    Q_INTERFACES(IPlugin)
public:
    PluginInfo info() const override;
    void initialize(QObject *services) override;
    void shutdown() override;
};
```

## Next Steps
- Define and version a stable `ServiceRegistry` interface passed to plugins.
- Add extension points (commands, menus, panels, language tools).
- Publish a plugin template repo.

## SDK Host Services

Plugins using the new SDK interface receive `IHostServices` in `initialize()`:

```cpp
bool initialize(IHostServices *host) override;
```

`IHostServices` provides typed access to IDE subsystems:

| Service | Method | Purpose |
|---------|--------|---------|
| Commands | `commands()` | Register/execute IDE commands |
| Workspace | `workspace()` | Workspace root, file operations |
| Editor | `editor()` | Active editor, selections |
| Views | `views()` | Create/toggle dock panels |
| Notifications | `notifications()` | Show info/warning/error toasts |
| Git | `git()` | Git status, diff, blame |
| Terminal | `terminal()` | Run commands in terminal |
| Diagnostics | `diagnostics()` | LSP diagnostics access |
| Tasks | `tasks()` | Task runner integration |
| Dock Manager | `docks()` | Add/remove/show/hide/pin dock panels |
| Menu Manager | `menus()` | Contribute menu items and context actions |
| Toolbar Manager | `toolbars()` | Create toolbars, add actions and widgets |
| Status Bar | `statusBar()` | Add status bar items and messages |
| Workspace Manager | `workspaceManager()` | Open folders/files, recent items |

### Service Registry Access

Plugins can register and query named services via `IHostServices`:

```cpp
host->registerService("myService", myQObject);
QObject *svc = host->queryService("myService");
auto *typed = host->service<IBuildSystem>("buildSystem");
```

### Core Plugin Services

| Service Key | Type | Registered By |
|-------------|------|---------------|
| `buildSystem` | `IBuildSystem` | Build plugin |
| `launchService` | `ILaunchService` | Build plugin |
| `buildToolbar` | `QToolBar` | Build plugin |
| `outputPanel` | `OutputPanel` | Build plugin |
| `runPanel` | `RunLaunchPanel` | Build plugin |
| `debugAdapter` | `GdbMiAdapter` | Core (DebugBootstrap) |

## AI plugins

Exorcist already supports agent/provider plugins through `IAgentPlugin`.

Detailed authoring guide: `docs/agent-provider-plugin-guide.md`

Current contract:

```cpp
class IAgentPlugin {
public:
        virtual ~IAgentPlugin() = default;
        virtual QList<IAgentProvider *> createProviders(QObject *parent) = 0;
};
```

Related core interfaces:

- `src/aiinterface.h`
- `src/agent/iagentplugin.h`

### Ownership rules

- Plugins own provider-specific integration.
- Core owns the shared chat runtime.
- Core owns the shared tool registry and tool execution pipeline.
- Plugins must not duplicate tool implementations that are already part of the shared platform.

### Core Brain Tools (available to all providers)

The agent platform includes tools that let the AI model manage persistent project knowledge:

| Tool | Operations | Purpose |
|------|-----------|---------|
| `manage_rules` | add, remove, list | Read/write project rules (conventions, style, safety). Rules are auto-injected into system prompts via `<project_rules>`. |
| `manage_memory` | add_fact, update_fact, forget_fact, list_facts, read_note, write_note, list_notes | Read/write memory facts and markdown notes. Facts injected as `<project_memory>`, notes as `<architecture_notes>`, `<build_notes>`, `<pitfall_notes>`. |
| `project_brain_db` | init, scan, query, learn, forget, status, execute | SQLite codebase index (files, symbols, dependencies, modules). |
| `scratchpad` | write, append, read, list, search, delete | Project-linked markdown notes outside the repo. |

Data storage: `.exorcist/rules.json`, `.exorcist/memory.json`, `.exorcist/notes/`, `.exorcist/project.db`

### Extension Interfaces (implemented)

These interfaces are defined in `src/agent/` and can be implemented by plugins:

| Interface | File | Purpose |
|-----------|------|---------|
| `IAgentSettingsPageProvider` | `iagentsettingspageprovider.h` | Plugin contributes a settings page widget to the AI Settings panel |
| `IProviderAuthIntegration` | `iproviderauthintegration.h` | Unified sign-in/sign-out lifecycle and auth status |
| `IChatSessionImporter` | `ichatsessionimporter.h` | Import vendor-specific remote session transcripts |
| `ILanguageIndexer` | `codegraph/ilanguageindexer.h` | Language-specific code indexing (classes, functions, imports, tests). Register via `CodeGraphIndexer::registerLanguageIndexer()`. See [docs/codegraph.md](codegraph.md) |

Plugins that implement these interfaces are discovered during
`AgentPlatformBootstrap::registerPluginProviders()` via `qobject_cast`.

### Plugin authoring goal

Adding a new model/backend plugin should mostly mean implementing `IAgentProvider` plus a thin plugin wrapper, while reusing the existing shared tool runtime and chat platform.

### Service expectations

AI plugins should expect to consume services such as:

- provider registry
- tool service
- session service
- auth/config services
- model catalog

Typed service interfaces are preferred over raw string lookups as the plugin API matures.

## C ABI Plugin API

For plugins that don't use Qt or need compiler-independent binaries, Exorcist
provides a stable C ABI. Plugins export four `extern "C"` functions and receive
a vtable of host service callbacks.

### Header

Include `src/sdk/cabi/exorcist_plugin_api.h` — pure C, no C++ or Qt dependency.
Optionally include `src/sdk/cabi/exorcist_sdk.hpp` for a C++ convenience wrapper.

### Required exports

```c
EX_EXPORT int32_t           ex_plugin_api_version(void);
EX_EXPORT ExPluginDescriptor ex_plugin_describe(void);
EX_EXPORT int32_t           ex_plugin_initialize(const ExHostAPI *api);
EX_EXPORT void              ex_plugin_shutdown(void);
```

### Host API surface

The `ExHostAPI` vtable provides ~40 callbacks covering:

| Service | Functions |
|---------|-----------|
| **Memory** | `free_string`, `free_buffer` |
| **Editor** | `editor_active_file`, `editor_selected_text`, `editor_insert_text`, `editor_open_file`, etc. |
| **Workspace** | `workspace_root`, `workspace_read_file`, `workspace_write_file`, `workspace_exists` |
| **Notifications** | `notify_info`, `notify_warning`, `notify_error`, `notify_status` |
| **Commands** | `command_register`, `command_unregister`, `command_execute` |
| **Git** | `git_is_repo`, `git_current_branch`, `git_diff` |
| **Terminal** | `terminal_run_command`, `terminal_send_input`, `terminal_recent_output` |
| **Diagnostics** | `diagnostics_error_count`, `diagnostics_warning_count` |
| **AI** | `ai_register_provider`, `ai_response_delta`, `ai_response_finished`, etc. |
| **Logging** | `log_debug`, `log_info`, `log_warning`, `log_error` |

### Minimal C example

```c
#include "exorcist_plugin_api.h"

static const ExHostAPI *g_api;

int32_t ex_plugin_api_version(void) { return EX_ABI_VERSION; }

ExPluginDescriptor ex_plugin_describe(void) {
    return (ExPluginDescriptor){
        .id = "org.example.hello", .name = "Hello", .version = "1.0.0",
        .description = "Hello World plugin", .author = "Example"
    };
}

int32_t ex_plugin_initialize(const ExHostAPI *api) {
    g_api = api;
    api->notify_info(api->ctx, "Hello from C ABI plugin!");
    return 1;
}

void ex_plugin_shutdown(void) { g_api = NULL; }
```

### AI provider via C ABI

Fill an `ExAIProviderVTable` and register it during init:

```c
int32_t ex_plugin_initialize(const ExHostAPI *api) {
    ExAIProviderVTable vtable = {
        .ctx          = my_provider_context,
        .id           = my_id,
        .display_name = my_name,
        .capabilities = my_caps,
        .is_available = my_available,
        .send_request = my_send_request,
        // ... fill remaining fields
    };
    api->ai_register_provider(api->ctx, &vtable);
    return 1;
}
```

Send streaming responses back via `api->ai_response_delta()` and
`api->ai_response_finished()`.

### C++ wrapper

`exorcist_sdk.hpp` provides RAII string management and typed service wrappers:

```cpp
#include "exorcist_sdk.hpp"

static exorcist::PluginHost *g_host;

int32_t ex_plugin_initialize(const ExHostAPI *api) {
    g_host = new exorcist::PluginHost(api);
    g_host->notifications().info("Hello from C++ plugin!");
    return 1;
}
```

## LuaJIT Script Plugins

For lightweight automation and utilities, Exorcist supports **Lua script plugins**
powered by LuaJIT v2.1. These run in a hardened sandbox with no file system,
network, or FFI access.

### Structure

```
plugins/lua/my-plugin/
├── plugin.json    # manifest with id, name, permissions, memoryLimit
└── main.lua       # entry script (must define initialize/shutdown)
```

### Manifest

```json
{
    "id": "org.example.my-plugin",
    "name": "My Plugin",
    "version": "1.0.0",
    "permissions": ["commands", "notifications", "editor.read"],
    "memoryLimit": 8
}
```

### Entry script

```lua
function initialize()
    ex.commands.register("myPlugin.run", "My Plugin: Run", function()
        local file = ex.editor.activeFile() or "(none)"
        ex.notify.info("Current file: " .. file)
    end)
end

function shutdown()
    ex.log.info("Goodbye!")
end
```

### Available API

All functions are under the `ex` global table:

| Module | Functions | Permission |
|--------|-----------|------------|
| `ex.commands` | `register(id, title, fn)`, `execute(id)` | `commands` |
| `ex.editor` | `activeFile()`, `language()`, `selectedText()`, `cursorLine()`, `cursorColumn()` | `editor.read` |
| `ex.notify` | `info(text)`, `warning(text)`, `error(text)`, `status(text, ms)` | `notifications` |
| `ex.workspace` | `root()`, `readFile(path)`, `exists(path)` | `workspace.read` |
| `ex.git` | `isRepo()`, `branch()`, `diff(staged)` | `git.read` |
| `ex.diagnostics` | `errorCount()`, `warningCount()` | `diagnostics.read` |
| `ex.log` | `debug(msg)`, `info(msg)`, `warning(msg)`, `error(msg)` | `logging` |
| `ex.events` | `on(name, fn)`, `off(name)` | `events` |
| `ex.runtime` | `memoryUsed()`, `memoryLimit()` | *(always available)* |

### Safety

- Per-plugin memory limit (default 16 MB, configurable)
- Instruction count limit (10M) prevents infinite loops
- `ffi`, `io`, `os`, `debug`, `package`, `require` all blocked
- Hot reload on file change (QFileSystemWatcher + 300ms debounce)

Full reference: [docs/luajit.md](luajit.md)

## JavaScript Plugin SDK (Ultralight JSC)

For rich scripting with a modern JavaScript engine, Exorcist ships a **JavaScript
Plugin SDK** as a Core Plugin (`plugins/javascript-sdk/`). It uses the JavaScriptCore
C API bundled with the Ultralight SDK. Two plugin types are supported:

- **Headless** — Pure JS execution (no DOM). Uses a bare `JSGlobalContext`.
- **HTML** — Full WebView with DOM + CSS + HTML rendered by Ultralight. The `ex.*`
  SDK is injected into the WebView context. Developer writes HTML/CSS/JS and uses
  `ex.*` for IDE communication. UI is rendered via Ultralight CPU renderer.

### Directory layout

**Headless plugin** (pure JS, no UI):
```
plugins/javascript/<plugin-name>/
├── plugin.json    # manifest (id, name, version, permissions)
└── main.js        # entry script (must define initialize/shutdown)
```

**HTML plugin** (WebView UI + ex.* SDK):
```
plugins/javascript/<plugin-name>/
├── plugin.json    # manifest with type: "html", contributions.views
└── ui/
    ├── index.html # HTML entry point
    ├── styles.css # styling (inlined at load time)
    └── main.js    # plugin logic using ex.* SDK + DOM
```

### `plugin.json` manifest

**Headless:**
```json
{
  "id": "org.example.my-plugin",
  "name": "My Plugin",
  "version": "1.0.0",
  "permissions": ["commands", "notifications", "editor.read", "workspace.read"]
}
```

**HTML:**
```json
{
  "id": "org.example.my-html-plugin",
  "name": "My HTML Plugin",
  "type": "html",
  "version": "1.0.0",
  "htmlEntry": "ui/index.html",
  "permissions": ["commands", "notifications", "editor.read"],
  "contributions": {
    "views": [
      { "id": "MyPanel", "title": "My Plugin", "location": "right", "defaultVisible": true }
    ]
  }
}
```

Available permissions: `commands`, `editor.read`, `notifications`, `workspace.read`,
`git.read`, `diagnostics.read`, `logging`, `events`.

View locations: `left`, `right`, `bottom`, `top`.

### JavaScript API surface (`ex.*`)

| Namespace | Methods | Required Permission |
|-----------|---------|-------------------|
| `ex.commands` | `register(id, title, callback)`, `execute(id)` | `commands` |
| `ex.editor` | `activeFile()`, `language()`, `selectedText()`, `cursorLine()`, `cursorColumn()` | `editor.read` |
| `ex.notify` | `info(text)`, `warning(text)`, `error(text)`, `status(text, timeoutMs)` | `notifications` |
| `ex.workspace` | `root()`, `readFile(path)`, `exists(path)`, `listDir(path)`, `openFiles()` | `workspace.read` |
| `ex.git` | `isRepo()`, `branch()`, `diff(staged)` | `git.read` |
| `ex.diagnostics` | `errorCount()`, `warningCount()` | `diagnostics.read` |
| `ex.log` | `debug(msg)`, `info(msg)`, `warning(msg)`, `error(msg)` | `logging` |
| `ex.events` | `on(name, callback)`, `off(name)` | `events` |

### Example plugin

```js
function initialize() {
    ex.commands.register('myPlugin.greet', 'Greet', function() {
        ex.notify.info('Hello from JavaScript!');
    });

    ex.events.on('fileSaved', function(path) {
        ex.log.info('Saved: ' + path);
    });
}

function shutdown() {
    ex.log.info('Plugin shutting down');
}
```

### Features

- Each JS plugin gets its own sandboxed `JSGlobalContext` (headless) or WebView (HTML)
- Permission-gated: only declared API namespaces are injected
- Hot reload on file change (QFileSystemWatcher + 300ms debounce) — headless plugins
- Multiple plugins can coexist independently
- Uses `JSEvaluateScript` for VM entry scope safety

### HTML plugin architecture

For HTML plugins, the SDK DLL (`libjavascript-sdk.dll`) handles everything:

1. Plugin manifest declares `"type": "html"` and `contributions.views`
2. `JsPluginRuntime` parses the manifest and creates `LoadedHtmlPlugin` entries
3. `JsPluginSdkPlugin` creates `UltralightPluginView` widgets (one per view)
4. Each view loads the HTML file with `<script>` and `<link>` resources inlined
5. After DOM is ready, the `ex.*` SDK is injected via `JsHostAPI::registerAll()`
6. The plugin's `initialize()` function is called
7. The view is registered as a dock panel via `IViewService::registerPanel()`

The JS developer writes standard HTML/CSS/JS and uses `ex.*` for IDE communication.
The Ultralight renderer (CPU mode) handles all rendering, input, and clipboard.

**Renderer sharing:** The process-wide Ultralight renderer is shared via
`qApp->property("__exo_ul_renderer")`. Whether the chat panel or a plugin
initializes Ultralight first, all views share the same renderer.

**Resource inlining:** Since Ultralight 1.4-beta crashes on `ulViewLoadHTML` and
`file://` URLs, external `<script src="...">` and `<link href="...">` references
are read from disk and inlined before loading via the about:blank + base64
injection workaround.
