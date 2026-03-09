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

### Extension Interfaces (implemented)

These interfaces are defined in `src/agent/` and can be implemented by plugins:

| Interface | File | Purpose |
|-----------|------|---------|
| `IAgentSettingsPageProvider` | `iagentsettingspageprovider.h` | Plugin contributes a settings page widget to the AI Settings panel |
| `IProviderAuthIntegration` | `iproviderauthintegration.h` | Unified sign-in/sign-out lifecycle and auth status |
| `IChatSessionImporter` | `ichatsessionimporter.h` | Import vendor-specific remote session transcripts |

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
