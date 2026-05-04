# Exorcist — Claude Agent Instructions

## MANDATORY TOOL PROTOCOL (applies to ALL agents and sub-agents)

These rules override all defaults. **No exceptions.** Violation of any rule below is an implementation bug.

---

## TOOL 1 — SOURCE GRAPH (Code Navigation & Indexing)

**Script:** `tools/source_graph_kit/source_graph.py`
**DB:** `tools/source_graph_kit/source_graph.db`
**Rebuild:** Runs automatically after every build. Manual: `python tools/source_graph_kit/source_graph.py build`
**Self-indexed:** The source graph indexes its own code (tools/ directory) — 16 files, 357+ functions. Use `context source_graph` to learn source_graph.py's own structure.

### ABSOLUTE RULES (ZERO TOLERANCE)

> ⚠️ **`grep`, `Grep`, `grep_search`, `semantic_search`, `find` (Bash), and `Read` are FORBIDDEN by default.**
> Use them ONLY when source_graph genuinely cannot answer (e.g., a file outside the indexed scope, or a pattern source_graph hasn't surfaced after a focused query).
> "I'll just grep quickly" is a violation. Run source_graph first. Every time.

1. **NEVER use `read_file`/`Read` without first querying the source graph.** Always call `context`, `func`, or `outline` first to locate exact line ranges. Then read only the needed range.

2. **NEVER use `grep_search`/`Grep`/`semantic_search` when the source graph can answer.** Source graph has FTS5 full-text search (`find`), regex search (`grep`), cross-references (`xref`), and call graphs (`who-calls`, `called-by`). Use IDE/Bash tools ONLY as a last resort, AFTER source_graph has been tried and failed for that specific query.

3. **NEVER read a file >100 lines without `context` or `outline` first.** Get the structure, identify the exact function/section, then read only that range.

4. **NEVER use `read_file` when `body <func>` can return the function directly.** The `body` command reads and returns function source — zero file reads needed.

5. **NEVER make multiple tool calls when `multi` can batch them.** Use `multi cmd1 --- cmd2 --- cmd3` to execute 2-5 queries in one invocation.

6. **NEVER use `read_file` for file reading when `read <file> [L1-L2]` exists.** The source graph's `read` command has fuzzy file resolution (handles partial names, project-relative paths) and is the preferred file reader.

7. **Source graph commands REPLACE IDE tools, not supplement them.** Use source graph FIRST and ONLY. Fall back to IDE tools (`grep_search`, `semantic_search`, `read_file`) if and only if the source graph cannot answer.

**Forbidden patterns:**
```
read_file mainwindow.cpp 1-3390      ← FORBIDDEN: read entire large file
grep_search "BuildPlugin"            ← FORBIDDEN: use `find BuildPlugin` or `xref BuildPlugin`
semantic_search "build system"       ← FORBIDDEN: use `find build` or `focus build`
read_file foo.cpp 1-500              ← FORBIDDEN without first calling context/func/outline
```

**Required patterns:**
```bash
source_graph.py context mainwindow           # understand structure first
source_graph.py body MainWindow::setupDocks  # read one function directly
source_graph.py outline mainwindow.h         # structural overview
source_graph.py read mainwindow.cpp L150-L200  # targeted read after context
source_graph.py multi context BuildPlugin --- xref CMakeIntegration --- who-calls autoDetect
```

### Command reference — Core commands

| Goal | Command |
|------|---------|
| Understand a file or class | `context <file\|class>` |
| Find a function + line range | `func <name>` |
| Read function body (no file read!) | `body <name>` |
| Full-text search all symbols | `find <term>` |
| Find a class hierarchy | `class <name>` |
| Find a method by name | `method <name>` |
| What depends on a class | `deps <class>` |
| Compact snapshot for agents | `focus <term> [budget]` |
| Minimal dependency slice | `slice <term> [budget]` |
| Assemble bug-fix context | `bundle bugfix <term>` |
| Assemble feature context | `bundle feature <term>` |
| Project overview + stats | `summary` |
| Bottleneck/optimization queue | `bottlenecks` |
| Function call edges | `calls <term>` |
| Impact of a change | `impact <term>` |
| Log architecture decision | `decide "<text>" --why Y` |
| Query decision log | `decisions [term]` |
| Find TODO/FIXME | `todo` |
| Run raw SQL | `sql "<query>"` |

### Command reference — AI-agent-oriented commands (NEW)

| Goal | Command | Replaces |
|------|---------|----------|
| Regex search in code + DB | `grep <pattern> [scope]` | `grep_search` tool |
| Read file with fuzzy resolution | `read <file> [L1-L2]` | `read_file` tool |
| Unified cross-reference | `xref <symbol>` | Multiple `grep_search` calls |
| Classes sharing same parent | `siblings <class>` | Manual DB queries |
| All implementors of an interface | `implements <interface>` | `find_class.py` |
| Reverse call graph | `who-calls <func>` | Manual `grep_search` |
| Forward call graph | `called-by <func>` | Manual `grep_search` |
| Full editing context for a line | `edit-context <file> <line>` | `read_file` + guessing |
| Structural outline (classes, funcs) | `outline <file>` | `read_file` entire header |
| Batch multiple commands | `multi <cmd1> --- <cmd2>` | Multiple tool calls |

### Command reference — Intelligent commands (AI memory + Git integrated)

| Goal | Command | Replaces |
|------|---------|----------|
| Unified source + memory context | `smart <topic>` | `context` + `ai_memory context` |
| Store insight to AI memory | `learn "<key>" "<value>" "<tags>"` | `ai_memory store` (no tool switch) |
| Recall AI memories | `recall <topic>` | `ai_memory context` (no tool switch) |
| Git history for a file | `history <file>` | `git log` in terminal |
| Recent git changes | `changes [file]` | `git log --stat` in terminal |
| Relationships between symbols | `relate <A> <B>` | Manual cross-ref queries |
| Suggest work items / tech debt | `suggest [scope]` | `gap_report` + manual TODO scan |
| Component completeness audit | `audit [scope]` | Manual Explore subagent multi-file reads |
| Data flow tracking | `dataflow [symbol]` | Manual grep for assignments |
| Invariant annotations | `invariant [symbol] [type]` | Manual assert/check searching |
| Hot path analysis | `hotpath [scope] [top_n]` | Manual call counting |
| CPU↔backend mappings | `backend-map [symbol] [type]` | Manual cross-file comparison |
| Security vulnerability scan | `exploit [type] [sev] [scope]` | Manual security audit |

### When to use which command

| Situation | Best command | Why |
|-----------|-------------|-----|
| "What is class X?" | `context X` or `xref X` | Full picture: deps, methods, location |
| "Where is function Y defined?" | `func Y` | Exact file + line range |
| "What calls function Y?" | `who-calls Y` | Reverse call graph from DB |
| "What does function Y call?" | `called-by Y` | Forward call graph from DB |
| "Show me the code of function Y" | `body Y` | Return full source, no file read needed |
| "I need to edit line 150 of file Z" | `edit-context Z 150` | Context: containing function, outline, includes |
| "What classes implement IPlugin?" | `implements IPlugin` | Recursive implementor tree |
| "Find all uses of pattern X" | `grep X [scope]` | Regex search through DB + files |
| "Overview of file Z structure" | `outline Z` | Classes, functions, enums with line ranges |
| "I need to understand + edit + test" | `multi context X --- who-calls Y --- outline Z` | Batch 3 queries in 1 call |
| "Quick overview before large task" | `focus <term>` or `bundle feature <term>` | Ranked snapshot or assembled context |
| "What do I know about topic X?" | `smart X` | Source graph + AI memory combined |
| "Remember this for later" | `learn "key" "value" "tags"` | Store directly, no tool switch |
| "What did I learn about X?" | `recall X` | AI memory query, no tool switch |
| "What changed in file Z recently?" | `history Z` or `changes Z` | Git log via source graph |
| "How are A and B related?" | `relate A B` | Hierarchy, calls, co-location, deps |
| "What should I work on next?" | `suggest [scope]` | Review queue + TODOs + header-only |
| "Is this subsystem complete?" | `audit [scope]` | Files, LOC, classes, SDK coverage, stubs |
| "Where does data flow from X?" | `dataflow X` | Track field writes, returns, assignments |
| "What invariants protect Y?" | `invariant Y` | Asserts, null checks, range checks, normalization |
| "What are the hottest functions?" | `hotpath` | Fan-in/out, depth, critical/GPU paths |
| "Is there a GPU version of X?" | `backend-map X` | CPU↔CUDA/OpenCL/Metal/Vulkan mappings |
| "Any security vulnerabilities?" | `exploit` | Command injection, path traversal, buffer overflow |

### Token economy rules (STRICT)
1. **`body` over `read_file`**: Always use `body <func>` instead of reading the full `.cpp` when you only need one function
2. **`func` → targeted read**: Use `func <name>` to get exact line ranges. If you must use `read_file`, read ONLY those lines
3. **`focus`/`slice` over multiple reads**: Use these to get compact ranked snapshots instead of reading multiple files
4. **`bundle` for complex tasks**: Use `bundle bugfix|feature|refactor <term>` to assemble all needed context in one call
5. **100-line rule**: Never read >100 lines without first calling `context` or `outline` to understand structure
6. **`find` before grep**: Use `find` (FTS5 search) before `grep_search` when searching for a symbol name
7. **`grep` replaces `grep_search`**: Use source graph's `grep <pattern>` instead of IDE's `grep_search` tool
8. **`read` replaces `read_file`**: Use source graph's `read <file> [L1-L2]` for file reading — it has fuzzy path resolution
9. **`multi` batching**: Combine 2-5 independent queries with `multi cmd1 --- cmd2` to save tool call overhead
10. **`xref` replaces scatter-search**: Use `xref <symbol>` instead of 3-4 separate searches for definition/callers/callees/usage
11. **`edit-context` before editing**: Before editing any file, call `edit-context <file> <line>` to understand the surrounding code

All commands run from the repo root:
```bash
python tools/source_graph_kit/source_graph.py <command> [args]
```

---

## TOOL 2 — AI MEMORY (My Persistent Memory)

**Script:** `tools/ai_memory/ai_memory.py`
**DB:** `tools/ai_memory/ai_memory.db`

This is my working memory. I use it to remember decisions, patterns, blockers,
and observations across sessions so I don't repeat mistakes or redo research.

### RULE: Check memory at the START of every task.

```bash
python tools/ai_memory/ai_memory.py context <topic>   # always first
python tools/ai_memory/ai_memory.py search "<query>"  # if context misses
```

### RULE: Store decisions and observations IMMEDIATELY when I learn them.

```bash
# Key architectural decision:
python tools/ai_memory/ai_memory.py store "decision:<topic>" "<what and why>" \
    --tag decision,<subsystem>

# Bug pattern or gotcha:
python tools/ai_memory/ai_memory.py store "bug:<class>-<issue>" "<description>" \
    --tag bug,fix,<subsystem>

# Architectural fact not visible in code:
python tools/ai_memory/ai_memory.py store "arch:<topic>" "<fact>" --tag insight,arch

# Blocker for later resolution:
python tools/ai_memory/ai_memory.py store "blocker:<topic>" "<description>" \
    --tag blocker
```

### Command reference

| Goal | Command |
|------|---------|
| Recall topic (FTS + key prefix + tags) | `context <topic>` |
| Full-text search | `search "<query>"` |
| Store or update | `store "<key>" "<value>" --tag t1,t2` |
| Recall exact key | `recall "<key>"` |
| List by tag | `list --tag <tag>` |
| All tags with counts | `tags` |
| Stats | `stats` |

### Common tags
`decision`, `bug`, `fix`, `pattern`, `insight`, `arch`, `blocker`,
`qt`, `cmake`, `lsp`, `agent`, `editor`, `plugin`, `ui`, `test`, `config`

### When NOT to store
- Things already visible in query output (source graph answers those)
- Info already in `docs/` or `memory/MEMORY.md`
- Resolved debugging observations with no future value

---

## EXECUTION ORDER FOR EVERY TASK (MANDATORY)

Every task — no matter how small — follows this order. Skipping steps is a protocol violation.

```
1.  ai_memory context <topic>                      ← recall prior knowledge
2.  source_graph focus <term>                      ← get compact project snapshot
3.  source_graph func / body / context / outline   ← find exact code locations
4.  source_graph read <file> L1-L2                 ← read only what's needed (NOT read_file)
5.  [Do the work]
6.  ai_memory store <key> <value> --tag ...        ← persist new knowledge
```

**Steps 1-3 are NOT optional.** Even for "simple" edits, query the graph first.

Sub-agents spawned via the `Agent` tool MUST follow this same order.

### Priority chain for information gathering

```
source_graph (body/context/func/find/xref/grep/outline)   ← FIRST (always)
    ↓ only if source graph has no answer
ai_memory (context/search)                                  ← SECOND
    ↓ only if neither has the answer
IDE tools (grep_search/semantic_search/read_file)           ← LAST RESORT
```

---

## BUILD & REINDEX

```bash
# Build (triggers automatic source graph reindex):
cmake --build build-llvm --config Debug

# Manual reindex (incremental):
python tools/source_graph_kit/source_graph.py build -i

# Full rebuild:
python tools/source_graph_kit/source_graph.py build
```

If the graph seems stale (new files missing), run `build -i` before proceeding.

### What is indexed

| Directory | Extensions | Content |
|-----------|-----------|---------|
| `src/` | `.cpp`, `.h`, `.hpp` | Core IDE code |
| `plugins/` | `.cpp`, `.h`, `.hpp` | Plugin code |
| `tests/` | `.cpp`, `.h` | Test code |
| `tools/` | `.py`, `.toml`, `.json`, `.md` | Build tools, source graph itself, AI memory |
| `docs/` | `.md` | Documentation |
| `profiles/` | `.json` | Profile configs |

**Self-indexing:** source_graph.py indexes its own code (357+ functions). Query `context source_graph` to understand the tool's own architecture.

---

## UI/UX Reference (MANDATORY)

All UI work must follow the Visual Studio 2022 dark theme reference:
- **Spec:** `docs/reference/ui-ux-reference.md` — full layout specification, color palette, UX principles
- **Image:** `docs/reference/vs-ui-reference.png` — authoritative visual reference screenshot
- Dark theme (#1e1e1e), dense info layout, left=explorers, right=tools, bottom=terminals/output
- Read the spec BEFORE implementing any visual component

## ARCHITECTURAL MANIFEST (MANDATORY — all models must follow)

This is the authoritative architectural vision. Every code decision must align
with these principles. If implementation and manifest diverge, align the code
to the manifest.

### Core Principle: Lightweight Reusable Container

The IDE is a **lightweight container** — a minimal shell that loads plugins and
provides abstract integration surfaces. The container's evolution is always
simple because it owns no concrete features.

```
MainWindow (container only)
├── 3 dock bar regions (left, right, bottom)
├── Menu bar rails
├── Toolbar rails
├── Status bar rails
├── Workspace/session lifecycle
├── ServiceRegistry (register/resolve by string key)
└── PluginManager (load/unload only, then steps aside)
```

**MainWindow does not know what plugins do internally. It does not care.**
MainWindow offers an abstract container layer and integration services so that
loaded plugins integrate into the environment and inject what they need via
interfaces.

### Core Principle: IHostServices — Direct Plugin Access

`IHostServices` connects **directly** to the plugin — not through PluginManager.
PluginManager's only role:
1. `QPluginLoader::load()` — load DLL
2. `plugin->setHostServices(host)` — hand over host reference
3. `plugin->initializePlugin()` — call init

After that, PluginManager is out of the picture. The plugin talks to the host
directly.

### Core Principle: Write Once, Reuse Everywhere

**Components are finished, self-contained systems with standardized interfaces.**
You write them once and reuse them as many times as needed, parameterized for
each context. Adding 100 new languages means 0 component rewrites.

| Reusable Component | What it does | How plugins use it |
|---|---|---|
| **Editor Component** | Full code editor with syntax, completion, multi-cursor | Initialize per language config, create as many instances as needed |
| **Output Panel** | Text output with error highlighting | Standard interface: give text → it prints. No customization needed |
| **Terminal** | PTY-based terminal emulator | Standard interface: send commands → see output |
| **UART Terminal** | Serial port terminal for embedded | Standard interface: open port → communicate |
| **Project Tree** | File/folder tree with filters | Standard interface: give root path + filters → shows tree |
| **Search Panel** | Workspace-wide search | Standard interface: give query → shows results |
| **Problems Panel** | Diagnostics/errors display | Standard interface: give diagnostics → shows them |
| **Debugger UI** | Breakpoints, watch, callstack | Standard interface: connect to debug adapter → visualize state |
| **Test Runner** | Test discovery and execution | Standard interface: give test framework → run and display |

These components live in **shared component libraries (DLLs)** grouped by type:
- **Components DLL** — foundational UI components (editors, panels, trees)
- **Instruments DLL** — developer tools (terminals, debugger UI, test runners)

### Core Principle: Plugin = Thin Glue Layer

A language plugin is narrow — it contains ONLY:

1. **Language-specific logic** (LSP configuration, language-specific rules)
2. **Language-specific UI** (if any — most languages need none)
3. **Import + initialization of shared components** with that language's parameters
4. **Wiring** — connecting language-specific logic to shared component interfaces

```
Plugin (DLL) — thin
├── Imports shared components from Component/Instrument DLLs
│   editor = EditorComponent(lang="cpp", syntax=cpp_grammar)
│   output = OutputPanel()  ← already finished, standard interface
│   tree   = ProjectTree(filters=["*.cpp","*.h"])
│   term   = Terminal()     ← already finished, standard interface
│
├── Language-specific logic (narrow)
│   LSP config for clangd
│   compile_commands.json detection
│   language-specific commands
│
├── Wiring (connect logic → components)
│   LSP diagnostics → output.appendLine(...)
│   LSP completion → editor.showCompletions(...)
│   build output → output.appendLine(...)
│
└── Optional: language-specific dock panel
    Written inside plugin, plugged into host dock interface
```

### Core Principle: Layered Plugin Ecosystem

```
Layer 1: IDE Container (MainWindow + PluginManager + IHostServices + ServiceRegistry)
         ↕ interfaces only
Layer 2: Shared Component Libraries (Components DLL, Instruments DLL)
         ↕ import and parameterize
Layer 3: General IDE Plugins (build, git, search, terminal, AI providers)
         ↕ services + events
Layer 4: Language Plugins (C++, Rust, Python, JS, Lua, embedded...)
         ↕ domain-specific
Layer 5: Community/Third-Party Plugins (extensions, integrations)
```

### Core Principle: ExoBridge = Global Shared Daemon

ExoBridge is NOT a plugin. It is a separate daemon process for resources that are
**global to the machine, not to a plugin or workspace**:
- Git tokens and OAuth credentials
- Auth token cache (one login benefits all IDE instances)
- MCP tool servers (stateless tool calls)
- Git file watchers (per-repo, not per-window)
- Future: package manager cache

If a resource depends on which project is open or which window is active, it
stays per-IDE-instance, NOT in ExoBridge.

### Enforcement Rules for AI Models

1. **Never put concrete feature logic in MainWindow.** Container only.
2. **Never duplicate a shared component.** If OutputPanel exists, use it — don't write another one.
3. **Never make a plugin fat.** If logic is reusable across plugins, extract to shared layer.
4. **Never bypass IHostServices.** Plugins access host through interfaces, never by direct internal reference.
5. **Check before creating:** Does a shared component already exist? Use `source_graph find <component>`.
6. **New component = shared by default.** If you write Terminal once, it must be reusable for next 100 plugins.
7. **Plugin-specific UI is the exception, not the rule.** Most plugins need zero custom UI — they compose shared components.
8. **IDE evolution is simple** because the container is small and stable. Complexity lives in plugins and shared components, not in the shell.

---

## CONTAINER vs PLUGIN — STRICT PLACEMENT RULES (zero tolerance)

These rules are **MANDATORY** before creating ANY new file or feature. Violation is a manifesto breach and must be reverted.

### Rule 1 — BEFORE writing any new code, answer this question

> *"Is this CONTAINER infrastructure, or is this a FEATURE for a specific technology/language/tool?"*

If the feature is for a specific **technology, language, build system, debugger, framework, file format, or external tool** → it goes to a **plugin DLL**. Period.

Examples:
- Qt class wizard, .ui editor, QML preview, Linguist, QSS editor → `plugins/qt-tools/`
- C++ LSP config, clangd integration → `plugins/cpp-language/`
- CMake/Ninja/MSBuild integration → `plugins/build/`
- GDB/LLDB debugger UI → `plugins/debug/`
- Git operations → `plugins/git/`
- Anthropic/OpenAI/Ollama providers → `plugins/<provider>/`

### Rule 2 — `src/` is the CONTAINER. Strictly limited to:

| Allowed in `src/` | Reason |
|---|---|
| `src/core/` | Pure abstractions (IFileSystem, IProcess, ITerminal) |
| `src/sdk/` | Plugin-facing SDK (IHostServices, IDebugAdapter, IBuildSystem) |
| `src/editor/` | Generic code editor component (no language code) |
| `src/lsp/` | LSP framework only — NOT language-specific configs |
| `src/agent/` | AI/agent orchestration (provider-agnostic) |
| `src/settings/` | Generic settings (User/Workspace scope) |
| `src/bootstrap/` | Lifecycle plumbing (services, managers) |
| `src/ui/` | Container chrome ONLY: menubar, toolbar rails, statusbar, welcome page, command palette |
| `src/project/` | Generic project/solution model (no language-specific wizards) |
| `src/search/` | Generic workspace search |
| `src/git/` | Generic VCS framework only (concrete git ops live in `plugins/git/`) |
| `src/forms/` | **FORBIDDEN.** Forms = Qt-specific = plugin |

Anything else → **plugin**.

### Rule 3 — FORBIDDEN file paths in `src/`

Adding a new file matching ANY of these patterns is a **violation** and must be placed in a plugin instead:

```
src/forms/**                       ← Qt forms = plugin
src/project/new*wizard.{cpp,h}     ← language/tech wizards = plugin
src/project/*template*.{cpp,h}     ← project templates = plugin (except generic provider interface)
src/ui/qt*.{cpp,h}                 ← Qt-specific dialogs = plugin
src/ui/kit*.{cpp,h}                ← Qt kits = plugin
src/ui/*helpdock*.{cpp,h}          ← feature docks = plugin
src/<language>/**                  ← anything language-specific = plugin
src/<vendor>/**                    ← anything vendor-specific (gdb/clang/cmake) = plugin
```

If you find yourself typing `src/forms/qt...` or `src/project/newqtclass...` — **STOP**. Move to `plugins/<technology>/`.

### Rule 4 — BEFORE creating any new feature

```bash
# 1. Does a plugin already exist for this technology?
ls plugins/<technology>/  # qt-tools/, cpp-language/, debug/, build/, git/, etc.

# 2. If yes → ADD to existing plugin. Do not create a parallel structure in src/.
# 3. If no → CREATE new plugin in plugins/<technology>/ via plugin scaffold.
# 4. NEVER add to src/ unless it is CONTAINER infrastructure (see Rule 2).
```

### Rule 5 — MainWindow may NEVER

Concrete prohibitions enforced in code review:

- ❌ `#include` a feature concrete header (e.g. `qthelpdock.h`, `formeditor.h`, `kitmanagerdialog.h`, `newqtclasswizard.h`)
- ❌ Construct a feature widget directly (e.g. `new QtHelpDock(this)`, `KitManagerDialog dlg(this)`)
- ❌ Reference a plugin's classes by type
- ❌ Know about specific file extensions, build systems, debuggers, languages, or external tools
- ❌ Have menu actions for tech-specific features (those are added by plugins via `IHostServices`)

If MainWindow needs to invoke a feature, the ONLY allowed mechanisms are:
- ✅ `ICommandService::executeCommand("plugin.command.id")` for actions
- ✅ `IDockService::registerDock(...)` for plugin-provided docks (plugin registers, container hosts)
- ✅ `IEditorRegistry::openEditor(path)` for file-type-specific editors (plugin registers factory)

### Rule 6 — Detection checklist (run on every MainWindow / `src/` edit)

Before committing any change to `src/mainwindow.{cpp,h}`:

```bash
# Show all includes — must contain ONLY container-level headers
grep -n '^#include' src/mainwindow.cpp | head -200
```

✅ Allowed includes: `pluginmanager.h`, `serviceregistry.h`, `sdk/hostservices.h`, `core/*.h`, `editor/editormanager.h`, `agent/*` (orchestrator only), `commandpalette.h`, `bootstrap/*.h`, `settings/scopedsettings.h`

❌ Forbidden includes (any of these = violation): `*helpdock.h`, `*formeditor*.h`, `*wizard.h` (except generic project wizards), `kit*.h`, `qt<feature>.h` for any concrete Qt feature

If you see ❌, **revert and move to plugin** before proceeding with any other work.

### Rule 7 — Sub-agent enforcement

When spawning sub-agents (`Agent` tool), the prompt MUST specify the target plugin path. Spawning a sub-agent to "add Qt help dock" without specifying *plugin location* is itself a violation.

✅ Correct: *"Add Qt help dock widget to `plugins/qt-tools/qthelpdock.cpp`. Plugin registers it via `IHostServices` dock API. Do not modify `src/mainwindow.cpp`."*

❌ Wrong: *"Add Qt help dock to the IDE."* (vague — sub-agent will likely put it in `src/ui/`)

### Rule 8 — When in doubt: PLUGIN

If unsure whether something is container or feature, **default to plugin**. The container is small, stable, and finished. New growth happens in plugins.

---

## PLUGIN LIFECYCLE — STRICT RULES (zero tolerance)

These rules govern **WHO** owns UI state and **WHEN** that state gets cleaned up. They are mandatory for every plugin and every container code path. Violations are an architectural breach — revert and fix before merging.

### Rule L1 — Plugins own every UI artifact they create

Every dock, toolbar, menu item, panel, status-bar widget, shortcut, command, and signal connection a plugin creates is **owned by that plugin**. The plugin is responsible for:

1. Constructing these artifacts in `initializePlugin()`
2. Tracking them on its own member fields (or via WorkbenchPluginBase auto-tracking)
3. Removing them in `shutdownPlugin()` — closeDock / removeAction / unregisterCommand

Container code (MainWindow, bootstrap classes, ServiceRegistry consumers) MUST NEVER iterate `dockWidgets()`, toolbar actions, or menu items to "clean up" plugin state. That is a Rule L1 violation.

### Rule L2 — Workspace lifecycle is plugin-broadcast

Workspace open / close events are broadcast to all active plugins through:

```cpp
class IPlugin {
    virtual void onWorkspaceOpened(const QString &root) {}
    virtual void onWorkspaceClosed() {}
};
```

`PluginManager::notifyWorkspaceChanged(root)` and `PluginManager::notifyWorkspaceClosed()` fire these on every active plugin. Plugins react in their handlers — set project root, clear caches, stop child processes.

MainWindow MUST NEVER:
- ❌ `lspService->stopServer()` directly on close
- ❌ `gitService->setWorkingDirectory({})` directly
- ❌ `m_dockManager->closeDock(d)` in a loop
- ❌ Touch a plugin-owned service to reset state

That state belongs to the plugin and only the plugin manages its own teardown.

### Rule L3 — Welcome / Start page is a plugin

The "no workspace" UI (welcome page, recent folders, getting-started cards) lives in `plugins/start-page/`, NOT in `src/ui/`. It activates on workspace closure, deactivates on workspace open. MainWindow has:

- ❌ no `WelcomeWidget` field
- ❌ no `m_centralStack` switch logic between welcome and editor
- ❌ no recent folders refresh code

Instead, StartPagePlugin contributes a Center-area dock and listens for workspace lifecycle events.

### Rule L4 — Dispatchers route to the active plugin

`GlobalShortcutDispatcher` and `CommandServiceImpl` are pure routing infrastructure. They never embed plugin-specific dispatch logic. When F5 fires, the dispatcher executes the registered command id (`build.debug`); whatever plugin owns that id at that moment handles it.

❌ No hardcoded "if debug session active, do X" anywhere in the dispatcher.
❌ No `if (commandId == "debug.stepOver") ...` switch statements in the container.

If a key sequence has no current owner (no plugin registered the command id), the press is silently dropped — that is correct behavior.

### Rule L5 — Container is small and finished

`src/mainwindow.cpp` is a container shell. Its only allowed responsibilities:

| Responsibility | Example |
|---|---|
| Construct application chrome | `new QMainWindow`, install menu/toolbar rails |
| Boot infrastructure | `PluginManager`, `HostServices`, `ServiceRegistry` |
| Forward workspace open/close as broadcast events | `m_pluginManager->notifyWorkspaceChanged(root)` |
| Window-level lifecycle | `closeEvent`, save/restore geometry, `QSettings` |

Anything else — git, build, debug, welcome, language, theme — belongs in a plugin.

### Rule L6 — Detection (run before every MainWindow / bootstrap edit)

```bash
# These calls in mainwindow.cpp / bootstrap files are red flags:
python tools/source_graph_kit/source_graph.py grep \
    "closeDock\|dockWidgets\(\)\|stopServer\(\)\|setWorkingDirectory" src/mainwindow.cpp
```

✅ Empty results = container is clean
❌ Any match = a plugin lifecycle responsibility leaked into the container; move it to the plugin's `onWorkspaceClosed` / `shutdownPlugin`.

### Rule L7 — `WorkbenchPluginBase` enforces ownership tracking

The base class auto-tracks artifacts created via its convenience helpers:

- `addPanel(...)`, `addMenuCommand(...)`, `addToolBarCommand(...)` — track owned ids
- `commands()->registerCommand(id, ...)` via `WorkbenchPluginBase::registerCommand` — tracked
- `shutdownPlugin()` base impl iterates owned ids and unregisters them

Plugins that bypass these helpers MUST track ownership themselves and clean up in `shutdownPlugin()`. Any plugin that calls `m_dockManager->addPanel(...)` directly without tracking is a Rule L1 violation.

### Rule L8 — Sub-agent prompts must specify lifecycle ownership

When spawning sub-agents to add UI to a plugin, the prompt MUST include:

> "The plugin owns this artifact's lifecycle. Track it on a member field and remove it in `shutdownPlugin()`. Do not modify MainWindow or any container code to manage it."

Vague prompts produce sub-agent code that "just works" by adding to MainWindow — a Rule L1 / L5 violation.

---

## UI/UX MANIFEST (mandatory for plugins)

All plugin and feature UI MUST follow [`docs/ux-principles.md`](docs/ux-principles.md) — the project-wide developer-friendly UX charter. Read it before writing or modifying any visual component. This is a **hard requirement**, not a guideline.

The 5 most important rules (the rest are in the doc):

1. **Keyboard-first** — every common action has a shortcut; mouse never required. Ctrl+Shift+P Command Palette is the universal escape hatch.
2. **Search-everywhere** — every list/tree with >10 items has a top search filter; property panels and settings are searchable.
3. **Inline > Modal** — modal dialogs only for destructive ops, multi-step wizards, or auth. All trivial edits are in-place (double-click text, popover for color, drag handles).
4. **Flat property inspectors** — flat sortable list with a search box and a "Modified / All" toggle. **No two-level category trees.** Type-aware inline editors only.
5. **Always undoable** — every state-mutating action goes through `QUndoStack` with a meaningful description; Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z all work.

**Subagents (Agent tool) MUST follow these principles when writing or refactoring any UI code.** Before adding a button, dialog, list, or panel: re-read `docs/ux-principles.md`. Before approving UI work, check it against the plugin author checklist in that document. Forbidden patterns (e.g., modal dialogs for trivial property changes, lists without search, buttons without tooltips) are documented there and must not be introduced.

Visual reference (colors, layout density, dock placement) is still governed by [`docs/reference/ui-ux-reference.md`](docs/reference/ui-ux-reference.md) and `vs-ui-reference.png`. The two documents are complementary: ux-principles.md = behavior, ui-ux-reference.md = visual style.

---

## Project Facts

- Build dirs: `build/` (MSVC), `build-llvm/` (Clang/Ninja, active)
- Active binary: `build-llvm/src/exorcist.exe`
- Qt6 / C++17 / Windows 10 primary target
- `src/mainwindow.cpp` — ~3980 lines (still large; container-only logic per architectural manifest)
- `src/mainwindow.h` — ~21 members after Phase D god-object decomposition
- Active chat panel: `ChatPanelWidget` (NOT `AgentChatPanel` — dead code)
- 74 agent tools registered; see `memory/agent-tools.md`
- Tests: `ctest --test-dir build-llvm`
- Qt6 gotcha: `QProcess::flush()` removed → use `waitForBytesWritten(0)`
- Source graph DB: 912 files, 6806 functions, 7539+ call edges, 1535 classes, 53206+ FTS entries
- Source graph indexes: `src/`, `plugins/`, `tests/`, `tools/`, `docs/`, `profiles/`

### Q2 2026 — Shipped feature facts (verify in source before changing)

**Debug plugin (`plugins/debug/`):**
- `MemoryDock` — `MemoryViewPanel` (`plugins/debug/memoryviewpanel.cpp`), real backend via `-data-read-memory-bytes`
- `DisassemblyDock` — `DisassemblyPanel` (`plugins/debug/disassemblypanel.cpp`), real backend via `-data-disassemble`
- Threads tab, Watch tab add/remove, Locals lazy expansion via GDB var-objects
- Conditional + hit-count + data breakpoints (watchpoints) wired through `WatchpointDialog` + breakpoint context menu
- Reverse debugging: `record full` → `reverse-step`/`reverse-next`/`reverse-continue`
- Pretty-printers enabled for STL types
- Stack frame switching from Call Stack panel
- Variable hover tooltip during debug session
- DebugPanel embedded toolbar removed — top toolbar (BuildToolbar) owns debug controls

**`IDebugAdapter` SDK (`src/sdk/idebugadapter.h`) — new virtual methods:**
- `readMemory(quint64 addr, int count)` + `memoryReceived` signal
- `disassemble(quint64 startAddr, int instructionCount, int mode)` + `disassemblyReceived` signal
- `addWatchpoint(const DebugWatchpoint&)` + `removeWatchpoint(int)` + `watchpointSet`/`watchpointRemoved` signals
- `stackSelectFrame(int frameId)` (formerly nonexistent — now mandatory)
- `startRecording`/`stopRecording`/`reverseStepOver`/`reverseStepInto`/`reverseContinue` (default no-op; GdbMiAdapter overrides)
- New struct types: `DebugWatchpoint`, `DisasmLine` (`Q_DECLARE_METATYPE`'d for queued signals)

**Editor (`src/editor/`):**
- Bracket matching (highlight on cursor adjacency)
- Indentation guides (faint vertical lines per indent level)
- Code folding by braces with gutter triangles
- TODO/FIXME/HACK/NOTE markers in gutter
- Multi-cursor: Ctrl+D add next match, Alt+Click add cursor
- Inline color swatches for hex codes + click-to-pick
- Image preview widget for png/jpg/svg/bmp/gif (`imagepreviewwidget.h/.cpp`)
- `SnippetEngine` (`src/editor/snippetengine.h/.cpp`) with C++/Py builtins + `${N:default}` expansion
- Whitespace toggle, Workspace symbols (Ctrl+T), Goto Line (Ctrl+G)

**LSP (`src/lsp/`, `plugins/cpp-language/`):**
- Format Document (Ctrl+Shift+F) via clangd
- Format-on-save (handled by `AutoSaveManager` for C/C++ files)
- Signature help popup with active-parameter highlight
- Quick Fix actions popup (Ctrl+.) via `textDocument/codeAction`

**System / Bootstrap (`src/bootstrap/`):**
- `GlobalShortcutDispatcher` (`globalshortcutdispatcher.h/.cpp`) — qApp event filter, key-sequence → command dispatch.
  Replaces per-widget `QShortcut` for global hot-keys; F10/F11 etc. registered via `registerShortcut(seq, cmdId)`.
- `MenuManagerImpl` — adopts existing menu by title instead of creating duplicates (dedup pass).
- `AutoSaveManager` — periodic dirty-editor save + format-on-save dispatch.
- `StatusBarManager` — line/col/encoding/indent indicators on right side, build/debug state indicator with VS color coding.

**Cross-DLL signal pattern (MANDATORY for SDK signals between DLLs):**
- PMF (pointer-to-member-function) `connect(obj, &T::sig, ...)` **silently fails** across DLL boundaries when the SDK type lives in a different binary.
- Use string-based `SIGNAL(...)`/`SLOT(...)` macros for any signal whose declaring class is defined in the SDK (`src/sdk/`) and emitted from a plugin (`plugins/*`).
- Examples: `IDebugAdapter::started/terminated/stopped`, `IBuildSystem::configureFinished`, etc.
- See `plugins/build/buildtoolbar.cpp:348-351` and `plugins/cpp-language/cpplanguageplugin.cpp:956-957` for canonical usage.
- Slots invoked via `SLOT(...)` MUST be declared in `private slots:` / `public slots:` (Qt moc requirement).

**Toolbar / Status bar layout:**
- `DockToolBar` height: **36 px fixed** (was 32 — buttons were clipped).
- Toolbar style: `Qt::ToolButtonIconOnly` everywhere (BuildToolbar, plugin toolbars), with QToolButton unified styling.
- Status bar: line/col, encoding, indent indicators on right; memory RSS + build/debug state on left.
- Sidebar repositioning + orphan-widget fixes — no widgets render at MainWindow `(0,0)` over the menu bar; dock sidebars no longer overlap menu bar items.

**UI shipped (in `src/ui/`):**
- `AboutDialog`, `GotoLineDialog`, `WorkspaceSymbolsDialog`, New From Template dialog, Run/Debug Configurations dialog
- `MarkdownPreviewPanel` dock (`src/ui/markdownpreviewpanel.h/.cpp`) — simple MD→HTML
- Recent Files menu in File menu (managed by `RecentFilesManager`, max 10)
- Window menu with open-tab list + tab navigation shortcuts
- Theme: light/dark toggle infrastructure (`Theme` + `SettingsPanel` toggles)
- Plugin Gallery card-style display + search filter
- All 6 panels polished: Search (regex/case/word + include/exclude + grouped), Outline (search/expand/sort/icons), Problems (severity filter + search + group-by-file), Git (staged/unstaged sections, inline diff, branch switcher), Terminal (multi-tab, profile selector, zoom), Welcome (recent folders).
