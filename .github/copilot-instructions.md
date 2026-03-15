# Exorcist IDE — Project Guidelines

Exorcist is a fast, lightweight, cross-platform Qt 6–based open-source IDE targeting Visual Studio–level capabilities with minimal footprint. Every contribution must preserve modularity, portability, and architectural clarity.

## Design Philosophy

The editor must always stay **lightweight**. Users who don't need a module must never pay its memory or startup cost. The modular plugin system gives users maximum freedom to compose exactly the environment they need — enable what they use, disable everything else. No wasted resources, no unnecessary complexity.

See [docs/core-philosophy.md](docs/core-philosophy.md) for the full Core IDE vs Core Plugins philosophy, activation model, and performance principles.
See [docs/development-principles.md](docs/development-principles.md) for the 10 development principles that guide how the project evolves.

## Architecture

- **Interface-first design**: All core systems (`IFileSystem`, `IProcess`, `ITerminal`, `INetwork`) are abstract interfaces in `src/core/`. Never depend on concrete implementations outside the core layer.
- **Plugin-first extension model**: New features belong in plugins unless they are fundamental OS abstractions. Plugins load via `QPluginLoader` and receive services through `ServiceRegistry`.
- **Service registry for loose coupling**: Services are registered/resolved by string key. Plugins must not reference each other directly — communicate through services.
- **AI as a plugin**: AI providers implement `IAIProvider` (see `src/aiinterface.h`). Core code must never hard-code an AI backend.
- **Layered architecture**: Core abstractions → Plugin system → UI layer. Dependencies flow downward only.

See [docs/plugin_api.md](docs/plugin_api.md) for plugin development and [docs/ai.md](docs/ai.md) for AI integration strategy.

## Core vs Plugin Boundary (STRICT)

The IDE executable (`src/`) contains everything required for a fully functional code editor — editor, language services, terminal, build, search, source control, and project management. These core subsystems expose **interfaces** so that plugins can interact with any part of the IDE. Plugins are optional extensions that users can enable or disable at runtime.

### What belongs in `src/` (Core)

The core IDE — all subsystems below ship as one executable and work together:

| Layer | Contents | Examples |
|-------|----------|---------|
| **App shell** | Window, tabs, dock framework, menus, status bar | `mainwindow.*`, `main.cpp` |
| **Editor** | Text editing, syntax highlighting, piece table | `editor/` |
| **Language Intelligence** | LSP client, Clangd manager, completion, hover, diagnostics | `lsp/` |
| **Terminal** | Terminal emulator (ConPTY/PTY), VT100+ emulation | `terminal/` |
| **Build System** | Task runner, build profiles, problem matchers | `build/` |
| **Source Control** | Git integration, blame, diff, staging | `git/` |
| **Search** | Workspace search, file search, regex search | `search/` |
| **Code Graph** | SQLite code intelligence — file/class/function indexing with line ranges, edges, Qt connections, services, tests, CMake targets, FTS5 | `codegraph/` |
| **Project** | Solution/project tree model, workspace management | `project/` |
| **MCP** | Model Context Protocol client for tool servers | `mcp/` |
| **Debug** | Debug adapter framework, GDB/MI, breakpoints, debug panel | `debug/` |
| **Remote / SSH** | SSH connections, remote FS browse, multi-arch probing, rsync, remote build | `remote/` |
| **Core abstractions** | OS interfaces, file system, process, network | `core/`, `plugininterface.h`, `aiinterface.h` |
| **Plugin system** | Plugin loader, service registry | `pluginmanager.*`, `serviceregistry.*` |
| **UI framework** | Command palette, theme engine, keymap | `commandpalette.*`, `thememanager.*`, `ui/` |
| **Logger** | Application logging | `logger.*` |
| **Agent framework** | Agent orchestrator, controller, interfaces, tools, chat UI | `agent/` (runtime + interfaces, **no providers**) |
| **ExoBridge** | Shared daemon — MCP servers, git watch, auth tokens | `server/`, `src/process/` |

Each core subsystem exposes interfaces (registered in `ServiceRegistry`) so plugins can:
- Query diagnostics, request completions, trigger formatting (via LSP interfaces)
- Read terminal output, run commands (via terminal interfaces)
- Get git status, diffs, blame (via git interfaces)
- Access search results, file listings (via search interfaces)
- Read build output, trigger builds (via build interfaces)

### ExoBridge — Shared Resource Boundary (STRICT)

**ExoBridge** is a persistent daemon process (`exobridge`) that manages resources shared across all Exorcist IDE instances on the same machine. Only resources that are **truly general-purpose services** — not tied to a specific workspace, solution, or editor session — belong here. If a resource depends on which project is open, which files are edited, or which window is active, it stays per-instance.

#### What MUST live in ExoBridge (`server/` + `src/process/`)

| Resource | Why shared | Status |
|----------|-----------|--------|
| **MCP tool servers** | Independent child processes, stateless tool calls | ✅ |
| **Git file watcher** | Per-repo, not per-window; one `QFileSystemWatcher` per repo | ✅ |
| **Auth token cache** | OAuth tokens, API keys — one login benefits all instances | ✅ |
| **Future: package manager cache** | Downloaded packages shared across workspaces | Planned |

#### What MUST stay per-IDE-instance (`src/`)

| Resource | Why per-instance |
|----------|-----------------|
| **LSP / Clangd** | Per-session protocol, per-file state, per-cursor context || **Workspace file index** | Per-workspace/solution — different projects have different file trees |
| **Search** | Per-workspace scope, tied to the open solution || **Terminal** | Per-window PTY, interactive I/O |
| **Editor state** | Cursor, selection, undo history — window-specific |
| **Build processes** | Per-task, per-profile, output tied to one window |
| **Debug sessions** | Per-target, per-breakpoint-set |

#### Rules

1. **No duplicate shared resources.** If a resource is listed in the "MUST live in ExoBridge" table, it is **forbidden** to create a per-instance version. All access goes through `BridgeClient` IPC.
2. **New shared resources go through ExoBridge.** When adding any new resource that could be shared (file watchers, caches, background scanners), implement it in ExoBridge first. If you're unsure whether something is shared, ask.
3. **ExoBridge services are callable via `BridgeClient::callService()`.** Each shared subsystem registers as a named service in `ExoBridgeCore`. IDE-side code calls through `BridgeClient`, never by spawning its own process.
4. **No direct process spawning for shared tools.** MCP servers, git watchers — all managed by ExoBridge. The IDE never calls `QProcess::start()` for a shared resource.
5. **Graceful degradation.** If ExoBridge is unreachable, the IDE must still function as a text editor. Shared features (MCP tools, git watch, auth tokens) may be unavailable but the IDE must not crash.
7. **Shared means workspace-agnostic.** A resource belongs in ExoBridge only if it provides the same service regardless of which project/solution is open. Per-workspace resources (indexer, search, LSP) stay per-instance.
6. **ExoBridge is not a plugin.** It is core infrastructure in `server/` and `src/process/`. It runs as a separate executable (`exobridge`) alongside the IDE.

### What belongs in `plugins/` (Modular)

Optional extensions that users can install, enable, or disable independently:

| Category | Plugin | Status |
|----------|--------|--------|
| **AI Providers** | Copilot, Claude, Codex, Ollama, BYOK | ✅ Already plugins |
| **Future extensions** | Additional language servers, debuggers, formatters, linters, themes, etc. | Designed as plugins from the start |

### Core Plugins (Bundled but Optional)

Core Plugins are official plugins bundled with the IDE that provide extended functionality. They follow the rule: **Bundled ≠ Active**. Just because a plugin is installed does not mean it runs.

| Category | Examples | Activation |
|----------|----------|-----------|
| **Language packs** | C/C++, Rust, Python, JS/TS, Go | Project detection (`CMakeLists.txt`, `Cargo.toml`, etc.) |
| **Tooling packs** | CMake, debugger integrations, test runners | Project detection + contextual (on first use) |
| **Integrations** | GitHub tools, database tools, container tools | Manual or contextual |
| **Advanced Tools** | AI agent tools, notebook support, analysis tools | Manual or contextual |

**Plugin Activation Model:**
1. **Manual** — developer explicitly enables/disables in settings
2. **Project detection** — auto-activate based on project files
3. **Contextual** — lazy-load only when functionality is invoked

**Zero-cost guarantee:** disabled plugins consume 0 CPU, 0 background threads, 0 RAM.

See [docs/core-philosophy.md](docs/core-philosophy.md) for the full philosophy.

### Rules

1. **No concrete AI provider in core.** `src/` must never contain a class that implements `IAgentProvider`. All AI providers live in `plugins/`.
2. **No `#include` of plugin headers in core.** Core code references only interfaces (`IPlugin`, `IAgentPlugin`, `IAgentProvider`), never concrete plugin classes.
3. **Communication through services.** Core subsystems register their interfaces in `ServiceRegistry`. Plugins resolve services by string key — never by direct reference.
4. **Plugin-provided UI.** Plugins can contribute dock widgets, settings pages, toolbar items, and menu actions through extension interfaces (`IAgentSettingsPageProvider`, etc.).
5. **Graceful absence.** The IDE must launch and function as a text editor even if zero plugins are loaded. No crashes, no missing features beyond what the plugins provide.
6. **One plugin, one directory.** Each plugin has its own directory under `plugins/` with its own `CMakeLists.txt`. Shared utilities go in `plugins/common/`.
7. **Core subsystems are not plugins.** Editor, LSP, terminal, git, build, search, project, and MCP are integral parts of the IDE. They live in `src/` and are always available. New features in these areas go directly into `src/`.

## Code Style

- **C++17** standard, compiled with CMake 3.21+.
- **Classes**: PascalCase (`MainWindow`, `ServiceRegistry`).
- **Interfaces**: PascalCase with `I` prefix (`IFileSystem`, `IPlugin`).
- **Member variables**: `m_` prefix (`m_fileModel`, `m_services`).
- **Header guards**: `#pragma once` — no `#ifndef` guards.
- **Ownership (ZERO TOLERANCE)**:
  - **`delete` is ABSOLUTELY FORBIDDEN.** Never write `delete` anywhere. Period.
  - **`new` is FORBIDDEN.** No exceptions by default. Smart pointers manage all lifetime.
  - **Default rule**: use `std::make_unique<T>(...)` or `std::make_shared<T>(...)` for ALL heap allocations — including QObject-derived classes.
  - **Member ownership**: all members that own heap memory MUST be `std::unique_ptr` or `std::shared_ptr`. Raw owning pointers are a bug.
  - **No ambiguous ownership.** Every heap allocation must have exactly one clear, automatic owner.
  - **Allowed patterns:**
    ```cpp
    auto widget = std::make_unique<QLabel>();       // ✅ unique_ptr owns it
    auto widget = std::make_unique<MyPlainObj>();   // ✅ unique_ptr owns it
    m_buffer = std::make_shared<Buffer>();          // ✅ shared_ptr owns it
    ```
  - **Forbidden patterns:**
    ```cpp
    auto *obj = new MyClass();         // ❌ FORBIDDEN
    auto *obj = new MyClass;           // ❌ FORBIDDEN
    new QLabel(parent);                // ❌ FORBIDDEN without explicit permission
    delete ptr;                        // ❌ FORBIDDEN always
    MyClass *m_thing = new MyClass();  // ❌ raw owning pointer member
    ```
  - **Edge cases require explicit user permission.** If a specific situation genuinely requires bare `new` (e.g., a Qt API that only accepts a raw pointer with parent ownership), you MUST:
    1. Stop and explain to the user WHY bare `new` is needed in this specific case.
    2. Show the exact line of code you want to write.
    3. Wait for explicit user approval before writing it.
    4. If the user says no, find an alternative design using smart pointers.
  - **Never assume permission.** Never silently write `new`. Never batch it into a larger change hoping it goes unnoticed. Each bare `new` usage requires its own separate approval.
- **Header/Source separation (ZERO TOLERANCE)**:
  - **No header-only files with logic.** Every `.h` file that contains function bodies, method implementations, constructors, or any executable code MUST have a corresponding `.cpp` file. Headers contain ONLY declarations.
  - **Allowed in headers:** class/struct declarations with member variables, pure virtual interfaces (`= 0`), `enum`/`enum class` definitions, `#define` macros, `constexpr` constants, `using`/`typedef` aliases, forward declarations, and trivial one-line getters that return a member (`T x() const { return m_x; }`).
  - **Forbidden in headers:** constructors with logic, method implementations (even if "short"), `static` functions with bodies, namespace-scoped functions with bodies, `Q_OBJECT` classes with implemented slots/signals handlers, layout/UI setup code, serialization logic (`toJson`/`fromJson`), any function body longer than a single `return m_member;` line.
  - **One header, one source.** If `foo.h` declares class `Foo`, implementations go in `foo.cpp`. No exceptions.
  - **Why:** Header-only files cause recompilation cascades, bloat object files through duplication, break linker expectations, and hide dependency chains. Separating declarations from definitions is fundamental C++ hygiene.
- **Includes**: Minimal in headers; prefer forward declarations. Group: Qt headers → std headers → project headers.
- **Error reporting**: Output `QString *error` parameters for fallible operations (see `IFileSystem`).
- **User-facing strings**: Wrap in `tr()` for translation support.
- **Platform code**: Use `Q_OS_WIN`, `Q_OS_MAC`, `Q_OS_UNIX` guards. Never use `#ifdef _WIN32` style.

## Build and Test

```bash
cmake --preset default        # Configure (Ninja, Debug)
cmake --build build           # Build
./build/exorcist              # Run
```

- Only required dependency: **Qt 6 Widgets**.
- Optional dependency stubs: `EXORCIST_USE_LLVM` CMake option.
- New dependencies must be MIT/BSD/public-domain compatible. Update [docs/dependencies.md](docs/dependencies.md) when adding any.

## Testing (STRICT — TDD)

Every new feature, class, or subsystem **must ship with tests in the same work session**. Tests are not a follow-up task — they are part of the implementation.

### Rules

1. **No code without tests.** Every new public class, function, or behavior change must have a corresponding test file in `tests/`. If you add `src/foo/bar.cpp`, you must add or update `tests/test_bar.cpp` in the same session.
2. **Test file naming.** Tests follow the pattern `test_<component>.cpp` (e.g., `test_multicursor.cpp`, `test_searchworker.cpp`).
3. **CMake registration.** Every new test must be added to `tests/CMakeLists.txt` with `add_executable`, `target_link_libraries`, `target_include_directories`, and `add_test`.
4. **Qt Test framework.** All tests use `QTest`. Test class inherits `QObject`, test methods are `private slots`, entry point is `QTEST_MAIN(ClassName)`.
5. **Test before integrate.** Write the engine/logic class first, test it in isolation, then integrate into UI. Never write UI-only code that can't be tested.
6. **Edge cases matter.** Test empty input, boundary conditions, error paths — not just the happy path. Aim for 5+ test cases per class minimum.
7. **Tests must pass before moving on.** Run `ctest --test-dir build-llvm --output-on-failure` after every implementation step. Do not proceed to the next feature if tests fail.
8. **Regression tests for bugs.** When fixing a bug, first write a test that reproduces it, then fix the code. The test prevents regression.

### What to test

| Code type | Test scope |
|-----------|-----------|
| New class / module | Unit tests for all public methods |
| Bug fix | Regression test reproducing the bug |
| New algorithm / logic | Input/output validation, edge cases |
| New tool (ITool) | `execute()` with valid/invalid params |
| New service | Registration, lookup, basic operations |
| Refactor | Existing tests must still pass, add coverage for changed behavior |

## Documentation (STRICT)

Documentation is **not optional** and must be maintained **alongside** implementation — never deferred to "later."

### Rules

1. **Document as you go.** Every new subsystem, interface, or significant feature must have corresponding documentation updated **in the same work session** it was implemented. No "we'll document it later."
2. **Roadmap reflects reality.** When a phase or feature is completed, [docs/roadmap.md](docs/roadmap.md) must be updated immediately to mark it done and briefly describe what was delivered.
3. **Architecture doc stays current.** When a new subsystem is added to `src/` (e.g., `debug/`, `agent/`), [docs/architecture.md](docs/architecture.md) must be updated with the new layer, its purpose, and its key interfaces.
4. **New subsystem = new doc.** Any new major subsystem (debug, agent platform, project brain, etc.) gets its own `docs/<subsystem>.md` describing: purpose, architecture, key types/interfaces, usage, and extension points.
5. **Plugin API changes are documented.** Any new extension interface (`IAgentSettingsPageProvider`, `IDebugAdapter`, etc.) must be reflected in [docs/plugin_api.md](docs/plugin_api.md).
6. **Dependencies tracked.** New third-party dependencies (even header-only) must be added to [docs/dependencies.md](docs/dependencies.md) with license info.
7. **Doc structure:** Keep docs concise and scannable. Use tables for type inventories, code blocks for interface signatures, and bullet lists for rules. Write in English (code comments) or Georgian (design rationale) — be consistent within each file.

### What to document

| Change type | Required doc update |
|-------------|-------------------|
| New `src/` subsystem directory | `architecture.md` + new `docs/<subsystem>.md` |
| New interface / abstract class | `architecture.md` layer table + subsystem doc |
| New plugin extension point | `plugin_api.md` |
| Completed roadmap phase/feature | `roadmap.md` status update |
| New dependency | `dependencies.md` |
| New data types / protocols | Subsystem doc (types table + protocol description) |
| UI panel / dock added | `architecture.md` app shell section |

## Conventions

- One interface, one header. Implementation in a separate `.cpp` with matching name (`ifilesystem.h` → `qtfilesystem.cpp`).
- Keep `main.cpp` minimal: app setup, theme, plugin loading, window show.
- Dock widgets in `MainWindow` are placeholders until a plugin replaces them.
- Follow the phased roadmap in [docs/roadmap.md](docs/roadmap.md) — do not skip phases.
- Commit messages: imperative mood, max 72 chars subject (`Add file search`, not `Added file search`).
- No `using namespace` in headers. Acceptable in `.cpp` files for `Qt` namespaces only.

## Code Graph — Codebase Intelligence (MANDATORY)

The project has a **SQLite-based code graph** that indexes the entire codebase. This is the authoritative source of truth about project structure, feature status, subsystem dependencies, and implementation gaps. **Always consult the code graph before making architectural decisions or answering questions about what exists/doesn't exist.**

### Tools

| Tool | Purpose | Usage |
|------|---------|-------|
| `tools/build_codegraph.py` | Full rebuild of `tools/codegraph.db` | `python tools/build_codegraph.py` |
| `tools/gap_report.py` | Actionable gap report (features, QObject, orphans) | `python tools/gap_report.py` |
| `tools/find_class.py` | Quick class/file/method/implementor/dependent lookup | `python tools/find_class.py ClassName` |
| `tools/check_deps.py` | Impact analysis before editing a file | `python tools/check_deps.py src/path/file.h` |
| `tools/gen_stub.py` | Generate .cpp skeleton from .h header | `python tools/gen_stub.py src/path/file.h [--dry]` |
| `tools/style_check.py` | Check coding conventions (bare new/delete, header-only) | `python tools/style_check.py [path]` |
| `tools/todo_extract.py` | Extract TODO/FIXME/HACK comments by subsystem | `python tools/todo_extract.py [path] [--summary] [--tag FIXME]` |
| `tools/test_coverage.py` | Map test gaps — which classes/subsystems lack tests | `python tools/test_coverage.py [subsystem] [--untested]` |
| `tools/api_surface.py` | Extract public API (signals, slots, methods) of a class | `python tools/api_surface.py ClassName [--signals]` |
| `tools/query_graph.py` | Interactive queries: context, search, functions, edges, features, subsystems, tests, services, connections, targets, SQL | `python tools/query_graph.py [mode] [args]` |
| `tools/verify_graph.py` | Data integrity verification | `python tools/verify_graph.py` |
| `tools/codegraph.db` | SQLite3 database (~35 MB) | Direct SQL queries via Python or `sqlite3` CLI |

### Database Schema

| Table | Contents | Key Columns |
|-------|----------|-------------|
| `files` | All indexed files (14k+) | `path`, `lang`, `lines`, `subsystem`, `has_qobject` |
| `classes` | All C++/TS classes (12k+) | `name`, `bases`, `is_interface`, `has_impl`, `file_id` |
| `methods` | Method declarations | `name`, `is_pure_virtual`, `is_signal`, `is_slot`, `has_body` |
| `includes` | `#include` edges | `file_id`, `included` |
| `implementations` | Header↔Source pairings (383+) | `header_id`, `source_id`, `class_name`, `method_count` |
| `features` | Feature detection results (56+) | `name`, `status`, `header_file`, `source_file` |
| `subsystems` | Subsystem stats (18) | `name`, `file_count`, `line_count`, `class_count`, `interface_count` |
| `subsystem_deps` | Inter-subsystem include edges | `from_subsystem`, `to_subsystem`, `edge_count` |
| `class_refs` | Class cross-references (19k+) | `from_class`, `to_class`, `ref_type` |
| `namespaces` | Namespace declarations | `file_id`, `name` |
| `file_summaries` | 1-line summary per file (14k+) | `file_id`, `summary`, `category`, `key_classes` |
| `function_index` | Functions with exact line ranges (3.9k+) | `file_id`, `name`, `qualified_name`, `start_line`, `end_line`, `class_name` |
| `edges` | Unified relationship graph (25k+) | `source_file`, `target_file`, `edge_type` (`includes`, `implements`, `inherits`, `tests`) |
| `qt_connections` | Qt signal/slot connections (408+) | `file_id`, `sender`, `signal_name`, `receiver`, `slot_name`, `line_num` |
| `services` | ServiceRegistry register/resolve calls | `file_id`, `service_key`, `reg_type` (`register`/`resolve`) |
| `test_cases` | QTest test methods (130+) | `file_id`, `test_class`, `test_method`, `line_num` |
| `cmake_targets` | CMake build targets (114+) | `target_name`, `target_type` (`executable`/`library`/`plugin`/`test`) |
| `fts_index` | FTS5 full-text search (30k+ entries) | `name`, `qualified_name`, `file_path`, `kind`, `summary` |

### Feature Status Values

| Status | Tag | Meaning |
|--------|-----|---------|
| `implemented` | `[OK]` | Has both `.h` and `.cpp` with matching class |
| `header-only` | `[HO]` | Header exists but no `.cpp` implementation found |
| `stub` | `[ST]` | `.cpp` exists but contains minimal/empty methods |
| `missing` | `[--]` | Neither header nor source found for the feature |

### Usage Rules

1. **Rebuild before major analysis.** If you've made structural changes (added/removed files, classes, or subsystems), rebuild the graph first: `$env:PYTHONIOENCODING="utf-8"; python tools/build_codegraph.py`
2. **Query don't guess.** When asked "does X exist?" or "what's the status of Y?", query the database — don't guess from memory or file names.
3. **Use SQL directly for ad-hoc questions.** Run inline Python with sqlite3 for one-off queries against `tools/codegraph.db`.
4. **Feature detection patterns live in `detect_features()`.** When adding a new tracked feature, add a tuple to the features list in `tools/build_codegraph.py`.
5. **Subsystem = top-level dir under `src/`.** The `detect_subsystem()` function maps paths to subsystem names (e.g., `src/agent/*` → `agent`, `src/editor/*` → `editor`, `plugins/copilot/*` → `plugin-copilot`).

### Development Workflow (MANDATORY)

Use the code graph tools **proactively** — not just when asked. This is how AI agents work efficiently with this codebase:

| When | Tool | Why |
|------|------|-----|
| **Before editing any `.h` or `.cpp`** | `check_deps.py <file>` | Know what breaks. See dependents, risk level, MOC/API warnings. |
| **Before implementing a new feature** | `find_class.py <name>` | Check if it already exists, find related classes, see subsystem structure. |
| **Before creating a new `.cpp`** | `gen_stub.py <header> --dry` | Preview the skeleton. Then `gen_stub.py <header>` to generate it. Never write boilerplate by hand. |
| **After adding/removing files or classes** | `build_codegraph.py` | Keep the graph current. Stale data = wrong decisions. |
| **When planning work** | `gap_report.py` | See what's missing, stubbed, or header-only. Prioritize effectively. |
| **When investigating a subsystem** | `find_class.py --sub <name>` | Get full class inventory + dependency map for that subsystem. |
| **Before integrating with a class** | `api_surface.py <Class>` | See public API, signals, slots. Understand the interface before wiring. |
| **Before writing tests** | `test_coverage.py [sub]` | See what's tested and what's not. Target high-value untested classes. |
| **When reviewing tech debt** | `todo_extract.py [path]` | Find all TODO/FIXME/HACK comments. Prioritize by subsystem. |
| **Before submitting changes** | `style_check.py [path]` | Catch bare `new`/`delete`, header-only violations, missing `.cpp` files. |
| **After any refactor** | `verify_graph.py` | Ensure graph integrity hasn't been broken. |
| **Full file context in one shot** | `query_graph.py context <file\|class>` | Summary + deps + rdeps + tests + functions with line ranges. One query instead of 5-6 tool calls (~80% fewer calls). |
| **Searching for symbols** | `query_graph.py search <query>` | FTS5 structured search — faster and better results than grep. |
| **Reading specific functions** | `query_graph.py functions <file\|class>` | Get exact line ranges, then `read_file L278-L354` vs reading entire file (~85% fewer tokens). |
| **Understanding relationships** | `query_graph.py edges <file>` | Unified view: who includes, tests, implements, inherits this file. |
| **Quick file lookup** | `file_summaries` table | 1-line summary per file — know what a file does without opening it. |

**Key principle:** Query → Understand → Plan → Implement → Verify. Never skip the first two steps.

### Common Queries

```python
# Feature status overview
SELECT name, status, header_file, source_file FROM features ORDER BY status, name;

# Header-only classes in src/ (need .cpp files)
SELECT c.name, f.path FROM classes c JOIN files f ON c.file_id = f.id
WHERE c.has_impl = 0 AND c.is_interface = 0 AND f.ext = '.h' AND f.path LIKE 'src/%';

# Subsystem stats
SELECT name, file_count, line_count, class_count, interface_count FROM subsystems ORDER BY line_count DESC;

# Dependency edges between subsystems
SELECT from_subsystem, to_subsystem, edge_count FROM subsystem_deps ORDER BY edge_count DESC;

# Find all classes in a subsystem
SELECT c.name, f.path FROM classes c JOIN files f ON c.file_id = f.id WHERE f.subsystem = 'agent';

# Interfaces without implementations
SELECT c.name, f.path FROM classes c JOIN files f ON c.file_id = f.id
WHERE c.is_interface = 1 AND c.has_impl = 0 AND f.path LIKE 'src/%';
```

### Enhanced Queries (New Tables)

```python
# Full context for a file (use query_graph.py context instead for formatted output)
SELECT fs.summary, fs.category, fs.key_classes FROM file_summaries fs
JOIN files f ON fs.file_id = f.id WHERE f.path LIKE '%chatinputwidget.cpp';

# Functions with line ranges (for targeted file reading)
SELECT fi.qualified_name, fi.start_line, fi.end_line, fi.line_count
FROM function_index fi JOIN files f ON fi.file_id = f.id
WHERE f.path LIKE '%agentcontroller.cpp' ORDER BY fi.start_line;

# All edges for a file
SELECT e.edge_type, f2.path FROM edges e
JOIN files f1 ON e.source_file = f1.id JOIN files f2 ON e.target_file = f2.id
WHERE f1.path LIKE '%chatinputwidget.cpp';

# Qt signal/slot connections in a subsystem
SELECT q.sender, q.signal_name, q.receiver, q.slot_name, f.path
FROM qt_connections q JOIN files f ON q.file_id = f.id WHERE f.subsystem = 'agent';

# FTS5 full-text search across all symbols
SELECT name, qualified_name, file_path, kind FROM fts_index
WHERE fts_index MATCH 'AgentController' ORDER BY rank LIMIT 20;

# Test cases for a specific test file
SELECT test_class, test_method, line_num FROM test_cases tc
JOIN files f ON tc.file_id = f.id WHERE f.path LIKE '%test_searchworker%';

# CMake targets by type
SELECT target_name, target_type FROM cmake_targets ORDER BY target_type, target_name;

# Files without summaries (orphans)
SELECT f.path FROM files f LEFT JOIN file_summaries fs ON f.id = fs.file_id WHERE fs.file_id IS NULL;
```

### PowerShell Note

When running Python scripts that output special characters on Windows, always set encoding:
```powershell
$env:PYTHONIOENCODING="utf-8"; python tools/build_codegraph.py
```

## MainWindow God Object — FREEZE (STRICT)

`MainWindow` currently has ~120+ member variables. This is known tech debt **scheduled for a dedicated decomposition session**.

### Rules until decomposition
1. **Do NOT add new member variables to `MainWindow`.** If a new feature needs state, put it in its own class/manager and access it through `ServiceRegistry` or a dedicated bootstrap object.
2. **Do NOT add new methods to `MainWindow`** that belong in a subsystem. Wire through callbacks or services instead.
3. **Do NOT refactor `MainWindow` piecemeal.** Decomposition must happen as a focused task with a clear plan (extract `EditorManager`, `DockBootstrap`, `BuildDebugBootstrap`, etc.), not ad-hoc member moves.
4. **Allowed:** Bug fixes that touch existing `MainWindow` code, and wiring new callbacks in the existing agent platform bootstrap section.
5. **Target:** Reduce from ~120 members to <80 by extracting cohesive manager classes. This is a Phase 12 task.
