# Exorcist — Roadmap

Self-hosting goal: Exorcist should be able to build and develop itself.
Priority rule: **typing never blocks** — all background work must be async/cancelable.

---

## Phase 0 — Foundation ✅ Done
- CMake + Qt 6 + Clang/MSVC presets
- Main window, tabs, dock panels (project / search / terminal / AI)
- Plugin API + ServiceRegistry
- Logger (thread-safe, timestamped → exorcist.log)
- Windows icon (4-size ICO)

---

## Phase 1 — Editor Core ✅ Done
- Project tree (QFileSystemModel), open-file double-click
- File read/write (IFileSystem abstraction)
- Large file lazy loading (chunked, scroll-triggered)
- Find/Replace panel (Ctrl+F, regex, case, whole-word)
- Line number gutter (auto-expands with digit count)
- Current-line highlight
- Syntax highlighting — 30+ languages (C/C++, Python, C#, Java, Dart,
  JS/TS, Rust, Go, Kotlin, Swift, Ruby, PHP, Lua, JSON, XML, HTML, SQL,
  CMake, YAML, TOML, Makefile, Dockerfile, GLSL, GraphQL, HCL, .pro …)
- Settings persistence (geometry + last folder via QSettings)
- Command palette (Ctrl+P file, Ctrl+Shift+P command)

---

## Phase 2 — Language Intelligence ✅ Done
- LspTransport + ProcessLspTransport (JSON-RPC 2.0 framing)
- ClangdManager (process lifecycle, crash/stop signals)
- LspClient (all LSP requests: completion, hover, signatureHelp,
  definition, formatting, rangeFormatting, diagnostics)
- CompletionPopup (Ctrl+Space + trigger chars `.` `->` `(`)
- LspEditorBridge (debounced didChange, Ctrl+Shift+F format)
- EditorView diagnostics (wavy underlines + gutter dots)
- Wired in MainWindow: folder open → clangd start → initialize → bridges
- Go-to-definition (F12) — opens file:line from `definitionResult`
- Find References (Shift+F12) — results in References panel
- Rename Symbol — `textDocument/rename` + workspace edits
- References panel dock
- Hover tooltip Markdown rendering (HoverTooltipWidget + MarkdownRenderer)
- Mouse hover triggers LSP textDocument/hover with 500ms debounce

---

## Phase 3 — Terminal + Build Tasks  ✅ Done
- Terminal panel — ConPTY backend (Win10+), QProcess fallback
- VT100/VT220/xterm emulation: 256 colors, scrollback (10K lines), alt screen
- Terminal context menu (Copy/Paste/Select All/Clear/Find/Zoom)
- Font size control (Ctrl+=/Ctrl+-/Ctrl+0)
- Search in scrollback (Ctrl+Shift+F, amber highlighting, prev/next)
- Tab rename (double-click)
- Build profiles — auto-detected from project files (CMake/Cargo/Make/npm/Python/.NET)
- Task profiles with JSON serialization (.exorcist/tasks.json)
- Environment variable substitution (${workspaceFolder}, ${env:VAR})
- Output panel — capture stdout/stderr, clickable error lines
- Problem matchers — GCC/Clang/MSVC/Rust/Python/TypeScript/ESLint
- Build diagnostics → editor wavy underlines bridge
- Elapsed time indicator during builds
- Edit menu with Undo/Redo/Cut/Copy/Paste/Select All/Find
- Run/Launch panel (F5/Shift+F5) with auto-detected targets
- Launch profiles from .exorcist/launch.json
- Auto-detection: CMake build dirs, Cargo, Python, npm, .NET, Go

---

## Phase 4 — Git Integration  ✅ Done
- Git status overlay on file tree (M / A / D / ? badges)
- Inline diff gutter (added/modified/removed lines)
- Diff viewer panel (side-by-side diff)
- Stage / unstage / commit UI panel (GitPanel dock)
- AI-generated commit messages
- Branch switcher + create branch + new branch button
- `git blame` per-line annotation
- File system watcher auto-refresh on .git changes
- Conflict file detection + AI conflict resolution
- Branch display in status bar

---

## Phase 5 — AI Assistant  ✅ Done
**Done:**
- IAgentProvider interface (streaming, capabilities, rich context)
- AgentOrchestrator (provider registry, lifecycle, routing)
- Chat panel (full Copilot-style UI with message groups, avatars, toolbar)
- IAgentPlugin (Qt plugin interface for provider implementations)
- Claude, Codex, Copilot provider plugins
- Inline completion engine (ghost text)
- Next Edit Suggestion (NES) engine
- Inline chat widget (selection → AI chat)
- Context builder (diagnostics, selection, file content)
- Agent platform with tools (file system, search, semantic search)
- Session store + model picker + session persistence (JSONL-backed)
- Prompt file loader (.github/copilot-instructions.md)
- Review annotations (inline code review)
- Proposed-edit diff panel (side-by-side, per-file accept/reject, Accept All/Reject All)
- Ollama local LLM provider plugin (streaming NDJSON, model discovery)
- Agent platform refactor: AgentProviderRegistry, AgentRequestRouter, ChatSessionService, ToolApprovalService
- Plugin extension interfaces: IAgentSettingsPageProvider, IChatSessionImporter, IProviderAuthIntegration
- Token usage tracking (prompt/completion/total display)
- File attachment marshalling (images, code files)
- SecureKeyStorage (DPAPI/Keychain/libsecret)
- Network monitor with offline/rate-limit toasts
- Async context gathering (workspace indexer, git status, diagnostics)

---

## Phase 5.5 — Project Brain + Debug  ✅ Done

### Project Brain (persistent workspace knowledge)
- ProjectBrainService: rules, facts, session summaries stored in `.exorcist/`
- BrainContextBuilder: assembles relevant knowledge for prompt injection
- MemorySuggestionEngine: auto-extracts facts from completed agent turns with toast UI
- MemoryBrowserPanel: 4-tab UI (Files/Facts/Rules/Sessions) with full CRUD
- Brain context injected into system prompt after custom instructions

See [docs/project-brain.md](project-brain.md)

### Debug Subsystem
- IDebugAdapter: abstract interface for debuggers (lifecycle, execution control, breakpoints, inspection)
- GdbMiAdapter: full GDB/MI protocol implementation via QProcess
- DebugPanel: toolbar (Launch/Continue/Step/Pause/Stop) + 5 tabs (Call Stack, Locals, Breakpoints, Watch, Output)
- Editor breakpoint gutter: click-to-toggle red circles, yellow debug line arrow
- Wired in MainWindow: DebugDock, View menu toggle, per-tab breakpoint signals

See [docs/debug.md](debug.md)

---

## Phase 6 — Settings + Theming  ✅ Done
**Done:**
- Settings dialog (Edit → Preferences, Ctrl+,)
  - Editor: font family/size, tab size, word wrap, minimap, line numbers
  - Appearance: theme selection (dark/light)
  - Persistent settings via QSettings
- AI Settings panel (General, Model, Tools, Context tabs)
- Theme engine (Fusion dark/light palette)
- KeymapManager with QSettings persistence + override loading
- Keyboard Shortcuts dialog (Edit → Keyboard Shortcuts, Ctrl+K Ctrl+S)
  - Search/filter, double-click to rebind, Reset All

- Custom workspace theme JSON (.exorcist/theme.json) — partial palette overrides

---

## Phase 7 — Quality + Performance  ✅ Done
- File watcher (auto-reload on external change) ✅
- Minimap (optional — thumbnail scroll overview) ✅
- Unit tests (QTest): ServiceRegistry, PieceTableBuffer, SseParser, ThemeManager — 40 tests passing
- Syntax highlighter keyword optimization (combined alternation regex)
- PieceTableBuffer wired to EditorView as shadow buffer (synced via contentsChange)
- Startup time: deferred plugin loading + QElapsedTimer instrumentation (logs ms-to-first-paint)
- Memory profiling: live RSS display in status bar (GetProcessMemoryInfo / /proc/self/statm)

---

## Phase 8 — Remote + SSH  ✅ Done
- **SshProfile** — connection profile struct with JSON serialisation (host, port, user, auth method, key path, remote work dir, env vars, detected arch/OS/distro)
- **SshSession** — QProcess-based wrapper around system `ssh`/`sftp`/`scp` commands; async command execution, directory listing, file read/write, file transfer
- **SshConnectionManager** — profile CRUD, persistence to `.exorcist/ssh_profiles.json`, session lifecycle management (connect/disconnect/disconnectAll)
- **RemoteFileSystemModel** — lazy-loading QAbstractItemModel for remote file trees (`ls -1aF` → expand-on-demand)
- **RemoteFilePanel** — UI panel: profile selector (add/edit/remove), connect/disconnect, remote terminal button, QTreeView for remote FS, architecture info label
- **Remote terminal** — `TerminalPanel::addSshTerminal()` opens interactive SSH session in a terminal tab
- **Remote build/run** — `OutputPanel::runRemoteCommand()` runs build commands over SSH with streaming output and problem matching
- **RemoteSyncService** — rsync wrapper for workspace ↔ remote directory sync; scp fallback for single files
- **Multi-architecture support** — `RemoteHostProber` auto-detects remote CPU arch (x86_64, AArch64/ARM64, ARM32, RISC-V 64/32, MIPS64, PPC64, s390x, LoongArch64), OS, distro, cores, memory, available compilers/build systems/debuggers/package managers; generates target triples for cross-compilation
- **MainWindow integration** — Remote dock (tabified with Project), View menu toggle, double-click→download→open, remote terminal tab launch

### Custom Docking System (Complete ✅)
- **Custom dock framework** — `exdock` namespace: `DockManager`, `ExDockWidget`, `DockArea`, `DockSplitter`, `DockOverlayPanel`, `DockSideBar`, `DockSideTab`, `DockTabBar`, `DockTitleBar`, `DockTypes`, `DockToolBarManager`
- **VS-style dock layout** — Horizontal root splitter [Left | Center vertical splitter | Right], center vertical [Top | Editor | Bottom]
- **Auto-hide system** — Sidebar tabs, slide-out overlay panel with pin/close, click-outside dismiss
- **Resize policies** — Stretch factors, minimum sizes, ratio-based save/restore
- **MainWindow integration** — All 17 dock panels migrated from QDockWidget to ExDockWidget, DockManager replaces QMainWindow native docking + DockAutoHideManager
- **State persistence** — JSON-based dock layout save/restore via DockManager (replaces QMainWindow::saveState/restoreState)

---

## Phase 9 — C++ Development Pipeline  ✅ Done
- **ToolchainManager** (`src/build/toolchainmanager.h/.cpp`) — local toolchain detection
  - Detects compilers (gcc, g++, clang, clang++, cl.exe) via PATH scanning
  - MSVC detection via `vswhere.exe` (Visual Studio Installer)
  - Debugger detection (gdb, lldb)
  - Build system detection (cmake, ninja, make, msbuild)
  - Target triple extraction (`-dumpmachine`) + clangd path resolution
  - Toolchain classification: GCC, Clang, MSVC, MinGW, LlvmMinGW
  - MSYS2 / LLVM MinGW path probing on Windows
- **CMakeIntegration** (`src/build/cmakeintegration.h/.cpp`) — CMake project lifecycle
  - Preset detection (CMakePresets.json / CMakeUserPresets.json)
  - Auto-detect existing build directories with CMakeCache.txt parsing
  - Configure / Build / Clean lifecycle via async QProcess
  - Target discovery from build output
  - compile_commands.json path management for clangd
  - Default Debug/Release configs with Ninja generator if available
- **BuildToolbar** (`src/build/buildtoolbar.h/.cpp`) — visual Build/Run/Debug toolbar
  - Configuration combo (Debug/Release/presets)
  - Target combo (discovered executables)
  - Configure / Build / Run / Debug / Stop / Clean buttons
  - Status label with build state feedback
  - Styled buttons (green Run, blue Debug, red Stop)
- **DebugLaunchController** (`src/build/debuglaunchcontroller.h/.cpp`) — build-before-debug pipeline
  - F5: Build (if configured) → launch debugger (gdb/lldb from toolchain)
  - Ctrl+F5: Build → run without debugger
  - Shift+F5: Stop debugging / running process
  - Debugger path auto-resolution from ToolchainManager
- **MainWindow integration** — keyboard shortcuts, toolbar, openFolder wiring
  - Ctrl+Shift+B: CMake build (with task profile fallback)
  - F5: Debug (continue if running, CMake debug launch with fallback)
  - Ctrl+F5: Run without debugging
  - Shift+F5: Stop (debugLauncher + runPanel)
  - openFolder: async toolchain detection, CMake auto-detect configs, toolbar refresh, compile_commands.json status

---

## Architecture invariants (never break these)
1. Typing never blocks — all analysis/IO is async or in a worker thread
2. Snapshot model — parsers/formatters work on a text copy, not live buffer
3. Cancelable jobs — every background task has a cancel path
4. Plugin isolation — exceptions in plugins must not crash the host
5. Core has no UI dependency — `src/core/` is pure Qt (no Widgets)

---

## Phase 10 — Plugin SDK Tiers  ✅ Done

### C ABI Plugin Layer
- `exorcist_plugin_api.h` — stable C header with versioned ABI, ~40 host API callbacks
- `exorcist_sdk.hpp` — C++ header-only wrapper (RAII, no Qt dependency)
- `cabi_bridge.h/cpp` — host-side bridge: C vtable ↔ Qt/C++ IHostServices
- AI provider registration via `ExAIProviderVTable` + `CAbiProviderAdapter`
- PluginManager auto-fallback: QPluginLoader → QLibrary C ABI resolution

### LuaJIT Scripting Runtime
- LuaJIT v2.1 statically linked, sandboxed per-plugin `lua_State`
- Custom allocator with per-plugin memory cap (default 16 MB, configurable)
- Instruction count hook (10M limit) prevents infinite loops
- Sandbox: FFI, io, os, debug, package, require all blocked
- Permission-gated `ex.*` API: commands, editor.read, notifications, workspace.read, git.read, diagnostics.read, logging, events
- `ex.runtime.memoryUsed()` / `memoryLimit()` — runtime memory introspection
- Event bridge (`ex.events.on/off`) — subscribe to IDE events from Lua
- Hot reload via QFileSystemWatcher (300ms debounce, per-plugin state reset)
- Traceback error handler on all pcall invocations
- 2 sample plugins: hello-world, word-count

See [docs/luajit.md](luajit.md)

---

## Phase 11 — Self-Hosting Dogfood  🔄 In Progress

**Goal:** Use Exorcist to develop Exorcist daily. Every pain point found while self-hosting becomes a bug fix or feature task.

### Pre-dogfood cleanup ✅
- [x] Dead TelemetryManager references removed (forward decl, member, include, CMakeLists)
- [x] `QDir::currentPath()` eliminated from outputpanel.cpp (2 occurrences)
- [x] `socket->flush()` Qt6 compat fixed in OAuthManager
- [x] BreadcrumbBar `delete` violations → `std::unique_ptr` + `deleteLater()`
- [x] `main.cpp` translator nodiscard warning suppressed
- [x] Todolist verified against actual codebase (inaccuracies corrected)

### Dogfood capability matrix
| Capability | Status | Notes |
|-----------|--------|-------|
| Open own project folder | ✅ | file tree + project manager |
| Edit C++/CMake/Markdown | ✅ | tree-sitter 7 grammars |
| LSP (clangd) intellisense | ✅ | completion, hover, go-to-def, references, rename |
| CMake configure + build | ✅ | preset detection, Ninja, problem matchers (GCC/Clang/MSVC) |
| Run executable | ✅ | RunLaunchPanel + OutputPanel |
| Debug (GDB) | ✅ | F5 build-before-debug, breakpoints, locals, call stack |
| Terminal | ✅ | ConPTY, VT100, search, tabs |
| Git (status/commit/blame/diff) | ✅ | GitPanel, inline diff gutter, branch switcher |
| Search (workspace/file/regex) | ✅ | SearchPanel, command palette |
| File watcher | ✅ | auto-reload on external change |
| AI chat (optional) | ✅ | Claude/Copilot/Codex/Ollama plugins |

### Remaining dogfood tasks
- [x] Self-hosting session: open `D:\Dev\Exorcist`, configure llvm-clang preset, build, run, verify
  - Startup: **774ms**, RSS: **116.7 MB** at boot → **265 MB** stable after LSP/plugins
  - LSP (clangd): initialized, 3 include paths from compile_commands.json
  - Copilot: authenticated, 15+ models available (Claude Opus 4.6, Sonnet 4.6, GPT-5.x, Gemini 3, Grok)
  - 6 Lua script plugins loaded, 79 agent tools registered
  - Session restore: 50+ JSONL session files, auto-restore with mode/title preservation
  - No crashes, no OOM — **stable at 265 MB** (VS Code: 800MB–2GB for same project)
- [ ] Document pain points and file issues
- [x] OAuthManager bare `new` → `std::unique_ptr`, PKCE flow complete
- [ ] MainWindow god object further decomposition (120+ members → target <80) — **deferred to Phase 12**

### Selfdogfood tool parity — completed
- [x] 79 agent tools registered (74 core + terminal_selection, terminal_last_command, test_failure, run_ide_command)
- [x] MCP auto-bridge covers Docker/Browser/Pylance tools via `McpToolAdapter`
- [x] Session auto-restore on startup (last session loads into chat panel, mode/title preserved)
- [x] 3 agent modes: Ask (read-only), Edit (safe mutate), Agent (autonomous loop)
- [x] SonarCloud CI: build-wrapper + coverage instrumentation + ReserchRepos excluded
- [x] `run_ide_command` tool — agent can list and execute any registered IDE command
- [x] MainWindow god object **FREEZE rule** added to copilot-instructions.md (decomposition deferred to Phase 12)
