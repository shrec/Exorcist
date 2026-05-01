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

1. **NEVER use `read_file` without first querying the source graph.** Always call `context`, `func`, or `outline` first to locate exact line ranges. Then read only the needed range.

2. **NEVER use `grep_search` or `semantic_search` when the source graph can answer.** Source graph has FTS5 full-text search (`find`), regex search (`grep`), cross-references (`xref`), and call graphs (`who-calls`, `called-by`). Use IDE tools only when source graph has no answer.

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
