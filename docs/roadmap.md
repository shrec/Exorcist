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

## Phase 2 — Language Intelligence ✅ Done (core)
- LspTransport + ProcessLspTransport (JSON-RPC 2.0 framing)
- ClangdManager (process lifecycle, crash/stop signals)
- LspClient (all LSP requests: completion, hover, signatureHelp,
  definition, formatting, rangeFormatting, diagnostics)
- CompletionPopup (Ctrl+Space + trigger chars `.` `->` `(`)
- LspEditorBridge (debounced didChange, Ctrl+Shift+F format)
- EditorView diagnostics (wavy underlines + gutter dots)
- Wired in MainWindow: folder open → clangd start → initialize → bridges

### Phase 2 — Remaining
- [ ] **Go-to-definition UI** — F12 opens file:line from `definitionResult`
- [ ] **LSP version guard** — discard `applyTextEdits` if doc version advanced
- [ ] **Find References** — `textDocument/references`
- [ ] **Rename Symbol** — `textDocument/rename`
- [ ] **Symbol outline** dock (breadcrumb / file symbols list)
- [ ] Hover tooltip polish (Markdown rendering)

---

## Phase 3 — Terminal + Build Tasks  🔴 Not started
- [ ] Terminal panel — QProcess PTY backend, ANSI color support
- [ ] Build profiles (cmake --build, cargo build, make …)
- [ ] Output panel — capture stdout/stderr, clickable error lines
- [ ] Problem matcher — parse compiler errors → jump to file:line
- [ ] Run/debug launcher (simple, no DAP yet)

---

## Phase 4 — Git Integration  🔴 Not started
- [ ] Git status overlay on file tree (M / A / D / ? badges)
- [ ] Inline diff gutter (added/modified/removed lines)
- [ ] Diff viewer panel
- [ ] Stage / unstage / commit UI
- [ ] Branch switcher
- [ ] `git blame` per-line annotation

---

## Phase 5 — AI Assistant  🟡 Infrastructure done, providers missing
**Done:**
- IAgentProvider interface (streaming, capabilities, rich context)
- AgentOrchestrator (provider registry, lifecycle, routing)
- AgentChatPanel (chat UI, provider selector, status dot)
- IAgentPlugin (Qt plugin interface for provider implementations)

**Remaining:**
- [ ] Anthropic Claude provider plugin (HTTP streaming, tool calls)
- [ ] Local LLM provider plugin (llama.cpp / Ollama HTTP)
- [ ] AgentChatPanel streaming display (delta chunks → live text)
- [ ] Inline suggestion mode (ghost text)
- [ ] Context injection (diagnostics, selection, file content)
- [ ] Proposed-edit diff view + accept/reject

---

## Phase 6 — Settings + Theming  🔴 Not started
- [ ] Settings dialog (font, tab size, theme, keybindings, LSP servers)
- [ ] Theme engine (color roles → paint, loaded from JSON/TOML)
- [ ] Light theme
- [ ] Font picker
- [ ] Keymap persistence + rebinding UI

---

## Phase 7 — Quality + Performance  🟡 Ongoing
- [ ] PieceTableBuffer wired to EditorView (replace QTextDocument buffer)
- [ ] Incremental syntax highlighting (dirty-range only rehighlight)
- [ ] File watcher (auto-reload on external change)
- [ ] Tests (zero coverage currently — start with EditorView + SearchService)
- [ ] Minimap (optional — thumbnail scroll overview)
- [ ] Memory profiling pass (target: < 50 MB idle on a C++ project)
- [ ] Startup time target: < 200 ms to first paint

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
