# Exorcist IDE — Project Guidelines

Exorcist is a fast, lightweight, cross-platform Qt 6–based open-source IDE targeting Visual Studio–level capabilities with minimal footprint. Every contribution must preserve modularity, portability, and architectural clarity.

## Design Philosophy

The editor must always stay **lightweight**. Users who don't need a module must never pay its memory or startup cost. The modular plugin system gives users maximum freedom to compose exactly the environment they need — enable what they use, disable everything else. No wasted resources, no unnecessary complexity.

Project motto: build a developer-friendly space that stays practical, uses minimal resources, and directly neutralizes real developer pain points.

This motto is enforceable, not decorative:

- If a feature does not reduce real developer friction, question whether it should exist.
- If a feature increases idle memory, startup cost, or background activity for users who did not ask for it, move it behind plugin activation, lazy loading, or shared infrastructure.
- If a workflow pain point appears in multiple plugins, fix it once in a shared base/service/component instead of duplicating the workaround.
- If a design is harder for plugin authors than it needs to be, keep simplifying the SDK and base layers until the standard path is obvious.

See [../docs/core-philosophy.md](../docs/core-philosophy.md) for the full Core IDE vs Core Plugins philosophy, activation model, and performance principles.
See [../docs/development-principles.md](../docs/development-principles.md) for the 10 development principles that guide how the project evolves.

## Architecture

- **Shell/container-first design**: The IDE shell is a container and bridge host, not a feature owner. `src/` should expose regions, lifecycle orchestration, contracts, and bridge access, not concrete subsystem workflows.
- **Interface-first design**: Core host surfaces (`IFileSystem`, `IProcess`, `ITerminal`, `INetwork`, `IHostServices`, dock/menu/toolbar/status interfaces) are abstract contracts. Never depend on concrete implementations across boundaries.
- **Plugin-first extension model**: Concrete feature systems belong in plugins. If a behavior has its own workflow, runtime state, commands, menus, toolbars, docks, status UI, or provider lifecycle, the default owner is a plugin.
- **Service registry for loose coupling**: Services are registered/resolved by string key. Plugins must not reference each other directly — communicate through services.
- **AI as a plugin**: AI providers implement `IAIProvider` (see `src/aiinterface.h`). Core code must never hard-code an AI backend.
- **Layered architecture**: Shell/container → shared contracts/bridge services → shared services/components/base plugin layer → domain plugins. Dependencies flow downward through contracts only.

See [../docs/plugin_api.md](../docs/plugin_api.md) for plugin development and [../docs/ai.md](../docs/ai.md) for AI integration strategy.

## Core vs Plugin Boundary (STRICT)

The IDE executable is the **host container**. Its responsibility is to provide abstract host surfaces, lifecycle orchestration, shared bridge access, and reusable platform primitives. It must not become the concrete owner of feature systems. Plugins are the feature owners and should manage their systems end to end.

### What belongs in `src/` (Core)

`src/` exists to host, abstract, bridge, and reuse. It is not where feature ownership should accumulate.

| Layer | Contents | Examples |
|-------|----------|---------|
| **App shell / container** | Window frame, layout host, dock/menu/toolbar/status regions, session/workspace lifecycle | `mainwindow.*`, `main.cpp`, bootstrap adapters |
| **Core abstractions and contracts** | OS interfaces, SDK boundaries, host interfaces, service contracts | `core/`, `plugininterface.h`, `aiinterface.h`, `sdk/` |
| **Plugin runtime** | Plugin loader, service registry, activation/profile orchestration | `pluginmanager.*`, `serviceregistry.*`, `profile/` |
| **Shared bridge access** | IPC client, shared-resource bridge wiring, cross-instance service access | `src/process/`, bridge bootstraps |
| **Shared services** | Reusable non-visual logic that is workspace-agnostic or feature-agnostic | logging, settings, persistence, command routing, shared diagnostics/session primitives |
| **Shared components** | Reusable visual building blocks with no domain ownership | dock framework, generic explorers, output/log viewers, reusable UI primitives |
| **Base plugin layer** | Standard helpers for plugins to own their feature surfaces consistently | `WorkbenchPluginBase`, `LanguageWorkbenchPluginBase` |
| **ExoBridge** | Shared daemon — MCP servers, git watch, auth tokens | `server/`, `src/process/` |

Concrete systems such as language workflows, build/debug/testing flows, provider runtimes, remote/devops tooling, AI integrations, and feature-specific workbench UI belong in plugins even when bundled. If a concrete system currently lives in `src/`, treat that as migration debt unless it is acting as a reusable host surface or abstract shared primitive.

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

Plugins are the concrete feature owners. They may be bundled or third-party, but ownership does not change: plugins define, activate, render, and manage their feature systems end to end.

| Category | Plugin | Status |
|----------|--------|--------|
| **Language systems** | C/C++, Rust, Python, JS/TS, Go, future language packs | Plugin-owned |
| **Tooling systems** | Build, debug, test, format, lint, VCS, analysis, remote/devops | Plugin-owned |
| **Providers / integrations** | Copilot, Claude, Codex, Ollama, BYOK, GitHub, databases, containers | Plugin-owned |
| **Feature workbenches** | Any concrete workflow UI, runtime, commands, docks, status surfaces | Plugin-owned |

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

See [../docs/core-philosophy.md](../docs/core-philosophy.md) for the full philosophy.

### Rules

1. **No concrete AI provider in core.** `src/` must never contain a class that implements `IAgentProvider`. All AI providers live in `plugins/`.
2. **No `#include` of plugin headers in core.** Core code references only interfaces (`IPlugin`, `IAgentPlugin`, `IAgentProvider`), never concrete plugin classes.
3. **Communication through services.** Core subsystems register their interfaces in `ServiceRegistry`. Plugins resolve services by string key — never by direct reference.
4. **Plugin-provided UI.** Plugins can contribute dock widgets, settings pages, toolbar items, and menu actions through extension interfaces (`IAgentSettingsPageProvider`, etc.).
5. **Graceful absence.** The IDE must launch and function as a text editor even if zero plugins are loaded. No crashes, no missing features beyond what the plugins provide.
6. **One plugin, one directory.** Each plugin has its own directory under `plugins/` with its own `CMakeLists.txt`. Shared utilities go in `plugins/common/`.
7. **The shell is not a feature owner.** `src/` may host contracts, bridges, and reusable shared primitives, but concrete workflows in language/build/debug/test/search/git/remote/AI domains must default to plugins, not core.
8. **Documentation and manifests are the source of truth.** When deciding UI ownership, activation, placement, or lifecycle, follow `docs/core-philosophy.md`, `docs/architecture.md`, `docs/plugin_api.md`, and the plugin manifest model before making any code change. If implementation and docs diverge, align the implementation to the documented architecture unless explicitly told otherwise.
9. **No cross-plugin copy-paste for shared behavior.** If the same workflow/helper/service access pattern appears in two or more plugins, the default action is to extract or extend a shared base class, shared service, or shared component before adding a third copy.
10. **Language/tooling plugins must inherit a shared plugin base.** C++, C#, Rust, Python, JS/TS, and similar workbench-facing plugins must build on the common base layer for standard IDE tooling access. Do not hand-roll repeated access to project tree, terminal, search, diagnostics, status, dock visibility, workspace path, or command plumbing in each plugin.
11. **Keep plugin classes thin.** Concrete plugins are for domain-specific behavior only. Standard workbench helpers, common lifecycle glue, standard dock/tool access, and repeated service lookup patterns belong in the base layer or shared services, not in each plugin implementation.
12. **When in doubt, extract downward, not outward.** If functionality is common to many plugins, first consider `WorkbenchPluginBase`, a dedicated abstract plugin base, or `plugins/common/`. Do not solve a design gap by duplicating code into C++, C#, Rust, Python, and other plugins independently.
13. **Agent enforcement rule.** The agent must treat repeated plugin boilerplate as an architectural bug, not as acceptable implementation detail. Before adding repeated menu/toolbar/service/dock/workspace code to a second or third plugin, stop and extract a reusable layer.
14. **Bridge-first rule for shared resources.** Cross-instance or workspace-agnostic resources must be reached through bridge interfaces. Core owns the bridge surface; plugins consume it through contracts.

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
- New dependencies must be MIT/BSD/public-domain compatible. Update [../docs/dependencies.md](../docs/dependencies.md) when adding any.

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
2. **Roadmap reflects reality.** When a phase or feature is completed, [../docs/roadmap.md](../docs/roadmap.md) must be updated immediately to mark it done and briefly describe what was delivered.
3. **Architecture doc stays current.** When a new subsystem is added to `src/` (e.g., `debug/`, `agent/`), [../docs/architecture.md](../docs/architecture.md) must be updated with the new layer, its purpose, and its key interfaces.
4. **New subsystem = new doc.** Any new major subsystem (debug, agent platform, project brain, etc.) gets its own `docs/<subsystem>.md` describing: purpose, architecture, key types/interfaces, usage, and extension points.
5. **Plugin API changes are documented.** Any new extension interface (`IAgentSettingsPageProvider`, `IDebugAdapter`, etc.) must be reflected in [../docs/plugin_api.md](../docs/plugin_api.md).
6. **Dependencies tracked.** New third-party dependencies (even header-only) must be added to [../docs/dependencies.md](../docs/dependencies.md) with license info.
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
- Follow the phased roadmap in [../docs/roadmap.md](../docs/roadmap.md) — do not skip phases.
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

## MainWindow Is ONLY a Container Shell (STRICT)

`MainWindow` is a **bare container** — a window frame that hosts the dock layout, menu bar, status bar, and central editor area. **All instruments, toolbars, dock panels, and development tools are owned and managed by their respective plugins**, never by MainWindow directly.

### Firmware Model (STRICT)

Treat the IDE like **firmware / BIOS** and plugins like the **operating system layer**:

- `src/` provides the container, shared interfaces, dock regions, menu bar, toolbar host, status bar host, command routing, and bridges to shared services.
- `src/` does **not** decide what concrete toolbars, menus, status widgets, or plugin workflows exist.
- Plugins decide what UI they contribute, where it appears, when it becomes visible, and how their internal environment behaves.
- Shared infrastructure and shared resources come from the IDE core and ExoBridge; feature-specific behavior lives inside the plugin that owns that feature.

### Manifest-Driven UI Ownership (STRICT)

The plugin manifest and host interfaces define UI ownership boundaries:

- A plugin may place menu items into the menu bar through `IMenuManager`.
- A plugin may create toolbars through `IToolBarManager`.
- A plugin may create/show/hide dock panels through `IDockManager` and `IViewContributor`.
- A plugin may publish status items through `IStatusBarManager`.
- A plugin may register commands and then surface them through its own menus/toolbars/context menus.

Therefore:

- `MainWindow` must only expose **regions and interfaces**, never plugin-specific menu contents or toolbar contents.
- If a feature needs a menu, toolbar, dock, or status UI, the default implementation belongs in the owning plugin, not in `MainWindow`.
- If an agent is unsure where UI logic belongs, the default answer is: **put the concrete UI ownership in the plugin and keep only the hosting/bridge layer in core**.

### What MainWindow MAY do

| Action | Example |
|--------|---------|
| **Host the dock/toolbar/menu/status infrastructure** | Create `DockManager`, provide `IToolBarManager`/`IMenuManager`/`IStatusBarManager` adapters |
| **Show core-subsystem docks on workspace open** | ProjectDock, GitDock, TerminalDock, AIDock, ProblemsDock |
| **Wire editor-level signals** | `navigateToSource` → open file, `debugStopped` → highlight line |
| **Route keyboard shortcuts** | Ctrl+B → build command, F5 → debug command (via CommandService) |
| **Delegate to ServiceRegistry** | Register/resolve services, never own plugin objects |

### What MainWindow MUST NEVER do

| Violation | Why it's wrong | Correct approach |
|-----------|---------------|-----------------|
| **Create or add plugin toolbars** | MainWindow doesn't know what plugins exist | Plugin calls `host->toolbars()->createToolBar()` + `addWidget()` |
| **Populate plugin-owned menus** | Menu content is feature ownership, not shell ownership | Plugin contributes actions/submenus through `host->menus()` or manifest menus |
| **Show plugin-contributed docks** | OutputDock, RunDock, DebugDock belong to their plugins | Plugin calls `host->docks()->showPanel()` in `initialize()` or on signal |
| **Connect plugin signals to dock visibility** | `connect(toolbar, buildRequested, showOutputDock)` couples MainWindow to plugin internals | Plugin wires its own signals → `host->docks()->showPanel()` |
| **Include plugin headers** | `#include "build/buildtoolbar.h"` in MainWindow | Never. Plugin headers are invisible to core |
| **Populate plugin-owned status bar UI** | Status content belongs to the feature that emits it | Plugin publishes its own status items through `host->statusBar()` |
| **Register plugin widgets as services for MainWindow to consume** | `registerService("buildToolbar", widget)` then MainWindow grabs it | Plugin adds its own toolbar via `IToolBarManager` |

### Plugin Self-Service Model

Every plugin manages its own UI lifecycle through `IHostServices`:

```cpp
bool MyPlugin::initialize(IHostServices *host) override
{
  // ✅ Plugin creates or contributes its own menu entries
  host->menus()->createMenu("myPlugin", tr("My Plugin"));

    // ✅ Plugin creates its own toolbar
    host->toolbars()->createToolBar("myTool", tr("My Tool"));
    host->toolbars()->addWidget("myTool", m_toolbar);

    // ✅ Plugin shows its own dock on activation
    host->docks()->showPanel("MyDock");

  // ✅ Plugin contributes its own status item
  host->statusBar()->addText("myPlugin.status", tr("Ready"));

    // ✅ Plugin wires its own signals → dock visibility
    connect(m_toolbar, &MyToolbar::actionTriggered, this, [host]() {
        host->docks()->showPanel("MyOutputDock");
    });

    return true;
}
```

### Shared Plugin Base Rule (STRICT)

For plugin authoring, the preferred layering is:

1. `IHostServices` and SDK interfaces define the hard boundary.
2. `WorkbenchPluginBase` or a more specialized abstract plugin base provides reusable standard behavior.
3. Concrete plugins implement only feature-specific behavior.

This means:

- The agent must prefer extending the shared base layer over duplicating the same host-service boilerplate across plugins.
- Standard IDE capabilities such as project tree access, workspace root access, terminal access, search/progress/status hooks, standard dock toggling, and command wiring must be surfaced through shared reusable plugin infrastructure.
- Language plugins should converge on a dedicated abstract language-plugin base when they share common workbench tooling behavior.
- The base layer must stay generic and reusable. It must not hard-code one concrete plugin's domain behavior.

### Anti-Duplication Enforcement (STRICT)

The following are treated as architecture violations unless there is an explicit user instruction otherwise:

- Copying the same `host()->workspace()`, `host()->editor()`, `host()->notifications()`, `host()->docks()`, or `host()->commands()` plumbing into multiple plugins when a base/helper can hold it.
- Re-implementing the same project-tree, standard dock, or standard command wiring logic in multiple language plugins.
- Adding a new plugin that bypasses the shared base layer for standard workbench behavior.
- Fixing a bug in one plugin by editing only that plugin when the same defect source exists in sibling plugins and belongs in a shared base/helper.

When the agent detects one of these patterns, the expected action is to refactor toward the shared base layer first, or to explicitly state why extraction is not appropriate.

### Available Plugin UI Interfaces

| Interface | Access via | Purpose |
|-----------|-----------|---------|
| `IToolBarManager` | `host->toolbars()` | Create toolbars, add widgets/actions |
| `IDockManager` | `host->docks()` | Add/show/hide/toggle dock panels |
| `IMenuManager` | `host->menus()` | Add menus and menu actions |
| `IStatusBarManager` | `host->statusBar()` | Add status bar items |
| `IViewContributor` | Plugin interface | Lazy-create dock content widgets |
| `ICommandService` | `host->commands()` | Register commands for palette/shortcuts |

### Enforcement

- **Code review check:** Any PR that adds plugin-specific wiring to MainWindow (toolbar creation, dock showPanel for plugin docks, signal connections to plugin objects) **must be rejected**.
- **Code review check:** Any PR that adds plugin-owned menu content, toolbar content, dock behavior, or status bar content directly to `MainWindow` **must be rejected** unless it is pure host/container infrastructure.
- **Grep check:** `grep -rn "showDock.*OutputDock\|showDock.*RunDock\|showDock.*DebugDock" src/mainwindow.cpp` must return 0 results.
- **Include check:** `grep -rn "#include.*plugins/" src/` must return 0 results.
