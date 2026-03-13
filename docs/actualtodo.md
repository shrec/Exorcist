# Exorcist IDE — სამუშაო გეგმა

## P0 — კრიტიკული (ახლავე)

### 1. MainWindow Decomposition (Phase 12) — ✅ DONE

**შედეგი:** 145 member → **69 member** (მიზანი იყო <80 — გადავაჭარბეთ)

| მეტრიკა | ადრე | შემდეგ | ცვლილება |
|----------|------|--------|----------|
| Member variables | 145+ | 69 | **-76** |
| mainwindow.h ხაზები | 320 | 213 | -107 |
| Bootstrap კლასები | 3 | 5 | +2 |

#### რა გაკეთდა:
1. **StatusBarManager** (`bootstrap/statusbarmanager.h/.cpp`) — 7 QLabel member + memory timer + copilot click handler
2. **AIServicesBootstrap** (`bootstrap/aiservicesbootstrap.h/.cpp`) — 28 AI/utility service member + internal wiring (OAuth↔Auth, APIKey↔KeyStorage, NetworkMonitor↔AuthIndicator, ChunkIndex signals)
3. **Dock pointer removal** — 19 `ExDockWidget*` member → `dock()` helper + `DockManager::dockWidget()` lookups
4. **Dead code cleanup** — 16 never-instantiated member declarations removed

---

### 2. ტესტების გაზრდა

**პრობლემა:** 40 test case ცოტაა 354 header / 139 cpp-ისთვის. კრიტიკული ქვესისტემები ტესტების გარეშეა.

**მიზანი:** 40 → 120+ test case

| ტესტი | რა ტესტავს | პრიორიტეტი | სტატუსი |
|-------|-----------|------------|---------|
| test_piecetable | Editor buffer | ✅ | 15 cases |
| test_serviceregistry | Service lookup | ✅ | 10 cases |
| test_sseparser | SSE events | ✅ | 5 cases |
| test_thememanager | Theme switching | ✅ | 10 cases |
| test_luascriptengine | LuaJIT sandbox | ✅ | 40 cases |
| test_lspclient | JSON-RPC framing, request/response | ✅ | 26 cases |
| test_searchworker | Workspace search, regex | ✅ | 11 cases |
| test_outputpanel | Problem matchers (GCC/Clang/MSVC/Rust) | ✅ | 16 cases |
| test_toolregistry | Tool registration, lookup, execution | ✅ | 5+ cases |
| test_contextbuilder | Context assembly, token limits | ✅ | 20 cases |
| test_gitservice | Git status parsing, diff | ✅ | 16 cases |
| test_multicursor | Multi-cursor engine | ✅ | 60 cases |
| test_projectmanager | Solution tree, file resolution | ✅ | 20 cases |
| test_terminalscreen | VT100 escape parsing | ✅ | 60 cases |
| test_treesitter_highlighter | UTF-8 offsets + tree-sitter integration | ✅ | 42 cases |
| test_testdiscovery | Test discovery service | ✅ | 13 cases |
| test_problemspanel | Problems panel aggregation | ✅ | 9 cases |
| test_workspacesettings | Hierarchical settings engine | ✅ | 28 cases |
| test_diffmerge | Diff explorer, merge editor, git extensions | ✅ | 17 cases |
| test_vimhandler | Vim modal editing engine | ✅ | 50 cases |
| test_remotelsp | Socket LSP transport, SSH port forward | ✅ | 11 cases |
| test_pluginmarketplace | Marketplace registry, install, uninstall | ✅ | 16 cases |
| test_syntaxhighlighter | Regex highlighter factory, formats, block comments | ✅ | 43 cases |
| test_highlighterfactory | HighlighterFactory tree-sitter/regex selection | ✅ | 24 cases |
| test_keymapmanager | KeymapManager register, override, reset, signals | ✅ | 19 cases |
| test_commandpalette | CommandPalette filter, file mode, command mode | ✅ | 17 cases |
| test_gitignorefilter | GitignoreFilter glob, negation, dir-only, loadFile | ✅ | 24 cases |
| test_contextpruner | ContextPruner add/remove/pin/prune, token budget | ✅ | 23 cases |
| test_autocompactor | AutoCompactor shouldCompact, estimateTokens, compact | ✅ | 19 cases |

**სულ: 29 test targets, 646+ test cases, 29/29 pass**

---

## P1 — მაღალი პრიორიტეტი

### 3. Multi-Cursor & Column Selection — ✅ DONE

**შედეგი:** MultiCursorEngine ძრავი + EditorView ინტეგრაცია + 60 test case

| ფუნქცია | სტატუსი |
|---------|---------|
| MultiCursorEngine (standalone, testable) | ✅ `src/editor/multicursorengine.h/.cpp` |
| insert/backspace/delete ყველა cursor-ზე | ✅ |
| Alt+Click — cursor-ის დამატება | ✅ |
| Alt+Shift+Drag — rectangular/column selection | ✅ |
| Ctrl+D — select next occurrence | ✅ |
| Ctrl+Shift+L — select all occurrences | ✅ |
| Column mode typing (rect selection-ში აკრეფა) | ✅ |
| Secondary cursor/selection rendering (paintEvent) | ✅ |
| Escape — secondary cursor-ების წაშლა | ✅ |
| Overlapping cursor merge | ✅ |
| test_multicursor.cpp — 60 test cases | ✅ 12/12 test targets pass |

### 4. Test Explorer Panel — ✅ DONE

**შედეგი:** TestDiscoveryService + TestExplorerPanel dock widget + 13 test cases

| კომპონენტი | სტატუსი |
|-----------|--------|
| TestDiscoveryService (CTest --show-only=json-v1 parsing) | ✅ `src/testing/testdiscoveryservice.h/.cpp` |
| TestExplorerPanel (dock widget, tree, Run All/Refresh) | ✅ `src/testing/testexplorerpanel.h/.cpp` |
| Per-test run (double-click), status icons, duration | ✅ |
| Integration with CMakeIntegration build directory | ✅ |
| test_testdiscovery.cpp — 13 test cases | ✅ |

### 5. Problems Panel (unified build/lint/runtime errors) — ✅ DONE

**შედეგი:** ProblemsPanel dock widget aggregating LSP diagnostics + build errors

| კომპონენტი | სტატუსი |
|-----------|--------|
| ProblemsPanel (dock widget, severity filter, count display) | ✅ `src/problems/problemspanel.h/.cpp` |
| LSP diagnostics aggregation (IDiagnosticsService) | ✅ |
| Build error aggregation (OutputPanel::problems()) | ✅ |
| Severity filtering (All/Errors/Warnings/Info) | ✅ |
| Double-click → navigate to file:line:col | ✅ |
| test_problemspanel.cpp — 9 test cases | ✅ |

### 6. Ghost Text Completion UX — ✅ DONE

**შედეგი:** Already fully implemented

| ფუნქცია | სტატუსი |
|---------|--------|
| Multi-line ghost text rendering (paintEvent splits on \n) | ✅ |
| Tab to accept, Escape to dismiss | ✅ |
| Ctrl+→ partial accept (word-by-word) | ✅ |
| 500ms debounce, Alt+\ manual trigger | ✅ |
| InlineCompletionEngine ↔ EditorView integration | ✅ |

---

## P2 — საშუალო პრიორიტეტი

### 7. Plugin Marketplace ✅
- **რა:** Plugin discovery, install, update, ratings
- **შესრულებული:** PluginMarketplaceService (registry JSON loading from file/URL, async download via QNetworkAccessManager, .zip extraction, uninstall, update check). Wired into PluginGalleryPanel’s installRequested signal. Bundled plugin_registry.json with 11 entries. ServiceRegistry integration. 16 tests pass.
- **ზემოქმედება:** Ecosystem growth

### 8. Multi-file Diff & Merge Editor ✅
- **რა:** Workspace-level diff explorer, commit comparison, 3-way merge conflict resolver
- **შესრულებული:** DiffExplorerPanel (revision picker, file list, side-by-side diff with highlighting), MergeEditor (conflict parser, ours/theirs/both resolution, save+stage), GitService extensions (showAtRevision, diffRevisions, changedFilesBetween, log). 17 tests pass.
- **ზემოქმედება:** Git workflow completeness

### 9. Workspace/Folder Settings ✅
- **რა:** Per-project settings (.exorcist/settings.json) + settings sync
- **შესრულებული:** WorkspaceSettings (global QSettings → workspace JSON overlay), typed accessors, QFileSystemWatcher for external changes, ServiceRegistry integration. 28 tests pass.
- **ზემოქმედება:** Team workflows, project-specific configuration

### 10. Vim/Emacs Keybindings ✅
- **რა:** KeymapManager-ის გაფართოება modal editing-ისთვის
- **შესრულებული:** VimHandler engine (Normal/Insert/Visual/VisualLine/Command modes), hjkl + w/b/e + 0/$ + gg/G + f/F + % motions, d/c/y/x operators with motions, numeric prefix, ex-commands (:w/:q/:wq/:e/:N), p/P paste, u/Ctrl+R undo/redo. Wired into EditorView. 50 tests pass.
- **ზემოქმედება:** Power user adoption

### 11. Remote LSP Bridging ✅
- **რა:** LSP server-ის remote host-ზე გაშვება SSH tunnel-ით
- **შესრულებული:** SocketLspTransport (TCP socket LSP transport with Content-Length framing), SshSession port forwarding (startLocalPortForward/stopPortForward), RemoteLspManager orchestrator (SSH → socat+clangd → port forward → socket connect). 11 tests pass.
- **ზემოქმედება:** Remote development completeness

---

## არსებული სისუსტეები

| #  | პრობლემა | სიმძიმე | სტატუსი |
|----|----------|---------|---------|
| 1  | MainWindow god object (145 member, 4750 ხაზი) | 🔴 Critical | ✅ 69 member (Phase 12 done) |
| 2  | ტესტების ნაკლებობა (40/139 cpp) | 🔴 Critical | ✅ 340+ cases, 17/17 pass |
| 3  | Multi-cursor არ არსებობს | 🔴 Critical | ✅ MultiCursorEngine + 44 tests |
| 4  | Test Explorer UI არ არსებობს | 🔴 Critical | ✅ TestExplorerPanel + TestDiscoveryService |
| 4b | Problems Panel არ არსებობს | 🔴 Critical | ✅ ProblemsPanel (LSP + build) |
| 5  | Extension Marketplace არ არსებობს | 🟡 Medium | ✅ PluginMarketplaceService + 16 tests |
| 6  | Multi-file diff / merge editor | 🟡 Medium | ✅ DiffExplorerPanel + MergeEditor + 17 tests |
| 7  | Chat UI header-only (20+ headers no .cpp) | 🟡 Medium | Ultralight-ზე გადასული |
| 8  | ExoBridge IPC ნაწილობრივი | 🟡 Medium | server/ daemon მუშაობს |
| 9  | Remote LSP bridging არ არსებობს | 🟡 Medium | ✅ SocketLspTransport + RemoteLspManager + 11 tests |
| 10 | OAuthManager token exchange | 🟢 Low | ✅ სრულად იმპლემენტირებული (PKCE + HTTP POST) |
