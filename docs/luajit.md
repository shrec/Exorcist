# LuaJIT Scripting Runtime

Exorcist embeds **LuaJIT v2.1** as a lightweight scripting tier for automation
plugins. Lua scripts are sandboxed, memory-limited, and restricted to read-only
IDE operations — they cannot register AI providers, write files, or access the
terminal.

## Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│  Plugin Tiers                                                      │
│                                                                    │
│  Tier 1 — C++ SDK     Full access, QPluginLoader, Qt dependency    │
│  Tier 2 — C ABI       Compiler-independent, QLibrary, no Qt       │
│  Tier 3 — LuaJIT ◄── Sandboxed scripting, read-only, lightweight  │
│  Tier 4 — DSL         Declarative config (future)                  │
└────────────────────────────────────────────────────────────────────┘
```

Lua plugins live in `plugins/lua/<plugin-name>/` next to the executable.
Each plugin gets its own `lua_State` — no shared globals between plugins.

## Directory Structure

```
plugins/lua/
├── hello-world/
│   ├── plugin.json      # manifest (id, name, permissions, memoryLimit)
│   └── main.lua         # entry script
├── word-count/
│   ├── plugin.json
│   └── main.lua
└── ...
```

## Plugin Manifest (`plugin.json`)

```json
{
    "id": "org.exorcist.hello-world",
    "name": "Hello World",
    "version": "1.0.0",
    "description": "A sample Lua plugin",
    "author": "Exorcist Team",
    "permissions": ["commands", "notifications", "logging"],
    "memoryLimit": 8
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | ✅ | Unique reverse-domain identifier |
| `name` | string | | Human-readable display name |
| `version` | string | | Semantic version (default "0.0.0") |
| `description` | string | | Short description |
| `author` | string | | Author name or organization |
| `permissions` | string[] | | API modules the plugin can access |
| `memoryLimit` | int | | Max memory in MB (default 16) |

## Permissions

| Permission | API Surface | Description |
|------------|-------------|-------------|
| `commands` | `ex.commands.*` | Register and execute IDE commands |
| `editor.read` | `ex.editor.*` | Read cursor position, selection, active file |
| `notifications` | `ex.notify.*` | Show info/warning/error toasts, status bar |
| `workspace.read` | `ex.workspace.*` | Read files, check existence, get root path |
| `git.read` | `ex.git.*` | Read branch, diff, repo status |
| `diagnostics.read` | `ex.diagnostics.*` | Read error/warning counts |
| `logging` | `ex.log.*` | Write to IDE log |
| `events` | `ex.events.*` | Subscribe to IDE events |

`ex.runtime.*` (memory query) is always available without permission.

## Lua API Reference

### Commands (`ex.commands`)

```lua
ex.commands.register(id, title, callback)  -- Register a command
ex.commands.execute(id)                    -- Execute a command → bool
```

### Editor — Read Only (`ex.editor`)

```lua
ex.editor.activeFile()    -- → string|nil  (current file path)
ex.editor.language()      -- → string|nil  (language id: "cpp", "python", ...)
ex.editor.selectedText()  -- → string|nil
ex.editor.cursorLine()    -- → int
ex.editor.cursorColumn()  -- → int
```

### Notifications (`ex.notify`)

```lua
ex.notify.info(text)
ex.notify.warning(text)
ex.notify.error(text)
ex.notify.status(text, timeout_ms)   -- status bar, default 5000ms
```

### Workspace — Read Only (`ex.workspace`)

```lua
ex.workspace.root()          -- → string|nil  (workspace root path)
ex.workspace.readFile(path)  -- → string|nil  (file contents)
ex.workspace.exists(path)    -- → bool
```

### Git — Read Only (`ex.git`)

```lua
ex.git.isRepo()       -- → bool
ex.git.branch()       -- → string|nil  (current branch name)
ex.git.diff(staged)   -- → string|nil  (diff output)
```

### Diagnostics — Read Only (`ex.diagnostics`)

```lua
ex.diagnostics.errorCount()    -- → int
ex.diagnostics.warningCount()  -- → int
```

### Logging (`ex.log`)

```lua
ex.log.debug(msg)
ex.log.info(msg)
ex.log.warning(msg)
ex.log.error(msg)
```

### Events (`ex.events`)

```lua
ex.events.on(name, callback)   -- Subscribe to an IDE event
ex.events.off(name)            -- Unsubscribe
```

Events are fired from C++ via `LuaScriptEngine::fireEvent()`. Planned events:

| Event Name | Args | Description |
|------------|------|-------------|
| `editor.save` | `(path)` | File saved |
| `editor.open` | `(path)` | File opened in editor |
| `editor.close` | `(path)` | File tab closed |
| `build.started` | `()` | Build task started |
| `build.finished` | `(success)` | Build completed |
| `git.commit` | `(message)` | Git commit made |

### Runtime (`ex.runtime`) — Always Available

```lua
ex.runtime.memoryUsed()   -- → int  (bytes currently allocated)
ex.runtime.memoryLimit()  -- → int  (max bytes allowed)
```

## Plugin Lifecycle

```lua
-- main.lua

-- Called after the script is loaded.
function initialize()
    ex.log.info("Plugin started!")
    ex.commands.register("myPlugin.hello", "Say Hello", function()
        ex.notify.info("Hello from Lua!")
    end)
end

-- Called when the plugin is unloaded or the IDE shuts down.
function shutdown()
    ex.log.info("Plugin stopped.")
end
```

Both `initialize()` and `shutdown()` are optional. If omitted, the plugin
simply runs its top-level code on load.

## Sandbox Restrictions

Lua plugins run in a **hardened sandbox**:

| Feature | Status | Reason |
|---------|--------|--------|
| `ffi` | ❌ Blocked | Arbitrary native code execution |
| `io` / `os` | ❌ Blocked | File system / process access |
| `debug` | ❌ Blocked | Sandbox escape via `debug.sethook` |
| `package` / `require` | ❌ Blocked | Loading arbitrary modules |
| `load` / `loadstring` / `dofile` | ❌ Blocked | Dynamic code execution |
| `rawget` / `rawset` | ❌ Blocked | Metatable bypass |
| `collectgarbage` | ❌ Blocked | GC manipulation |
| `jit` | ❌ Blocked | JIT control |
| `string` / `table` / `math` / `bit` | ✅ Available | Safe standard libraries |
| `print` / `type` / `tostring` / `pairs` | ✅ Available | Safe builtins |

## Memory Management

Each plugin has its own **memory budget** enforced by a custom allocator:

- Default limit: **16 MB** per plugin (configurable via `"memoryLimit"` in plugin.json)
- When the limit is exceeded, the allocator returns `NULL`
- LuaJIT raises `LUA_ERRMEM` internally, which `lua_pcall` catches
- **The IDE never crashes** — the plugin simply gets an error
- Plugins can query their own usage: `ex.runtime.memoryUsed()`

## Instruction Limiting

A Lua hook fires every **10 million instructions** and aborts the script with
`luaL_error()`. This prevents infinite loops from freezing the IDE.

## Hot Reload

When hot reload is enabled (automatic for loaded plugins):

1. `QFileSystemWatcher` monitors each plugin's directory
2. On file change, a **300ms debounce timer** starts
3. After debounce: `shutdown()` → `lua_close()` → fresh `lua_State` → re-load `main.lua` → `initialize()`
4. Memory budget resets on reload
5. All event subscriptions are re-established by the plugin's `initialize()`

This enables live development: edit `main.lua`, save, see changes instantly.

## Error Handling

- All `lua_pcall` calls use a **traceback error handler** (like the game server pattern)
- Stack traces include file name and line numbers
- Errors are logged with the plugin ID prefix: `[LuaJIT] org.example.plugin: error at main.lua:42`
- Errors in one plugin never affect other plugins (isolated states)

## Key Types

| Type | File | Purpose |
|------|------|---------|
| `MemoryBudget` | `luascriptengine.h` | Per-plugin memory tracking (used/limit/oomHit) |
| `LuaPluginInfo` | `luascriptengine.h` | Plugin manifest data |
| `LuaPermission` | `luascriptengine.h` | Permission flag enum (8 flags) |
| `LoadedLuaPlugin` | `luascriptengine.h` | Runtime state per plugin |
| `LuaScriptEngine` | `luascriptengine.h` | Engine — loads, runs, reloads plugins |
| `LuaHostAPI` | `luahostapi.h` | Binds IHostServices into `ex.*` Lua tables |

## Source Files

| File | Purpose |
|------|---------|
| `src/sdk/luajit/luascriptengine.h` | Engine class, types, permissions |
| `src/sdk/luajit/luascriptengine.cpp` | Engine implementation (sandbox, allocator, hot reload) |
| `src/sdk/luajit/luahostapi.h` | Host API binding declarations |
| `src/sdk/luajit/luahostapi.cpp` | Host API binding implementations (16+ C functions) |
