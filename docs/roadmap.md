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

## Phase 5 — AI Assistant  🟡 Core done, polishing
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
- Session store + model picker
- Prompt file loader (.github/copilot-instructions.md)
- Review annotations (inline code review)\n- Proposed-edit diff panel (side-by-side, per-file accept/reject, Accept All/Reject All)

- Ollama local LLM provider plugin (streaming NDJSON, model discovery)

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

## Phase 8 — Remote + SSH  🔴 Future
- SSH profiles + connection manager
- Remote file browser + edit
- Remote build / run + log streaming
- Optional file sync (rsync-style)

---

## Architecture invariants (never break these)
1. Typing never blocks — all analysis/IO is async or in a worker thread
2. Snapshot model — parsers/formatters work on a text copy, not live buffer
3. Cancelable jobs — every background task has a cancel path
4. Plugin isolation — exceptions in plugins must not crash the host
5. Core has no UI dependency — `src/core/` is pure Qt (no Widgets)
