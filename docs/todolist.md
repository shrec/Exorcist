# Exorcist — სამუშაო სია

> ბოლო განახლება: 2026-03-12 (P0/P1 tasks completed)
> წყარო: ტექნიკური აუდიტი (კოდის პირდაპირი ანალიზი + ვერიფიკაცია)

---

## კრიტიკული (დაუყოვნებლივ)

- [x] **TreeSitterHighlighter — incremental parse გამოსწორება**
  `onContentsChange()` არ იძახებდა `ts_tree_edit()` — tree-sitter-ი „incremental"-ს ყოველ keystroke-ზე full reparse-ს ახდენდა.
  `highlightBlock()` — `toPlainText()` ყოველ block-ზე + `fromUtf8().length()` ყოველ leaf node-ზე (heap allocation hot path-ში).
  → გამოსწორდა: `m_blockByteOffsets` cache, `ts_tree_edit()` სათანადო გამოძახება, allocation-free byte↔char კონვერსია.

- [x] **searchpanel.cpp: `QDir::currentPath()` constructor-ში**
  არქიტექტურის წესი: `QDir::currentPath()` არ გამოიყენება project root-ად initialization-ის გარეთ.
  → გამოსწორდა: `m_rootPath` ინიციალიზდება `{}` (ცარიელი სტრიქონი, `setRootPath()` ასწორებს).

- [x] **mainwindow.cpp: მანიფესტ #9 დარღვევა — TelemetryManager**
  მანიფესტი: "No telemetry, no analytics, no phoning home."
  → `m_telemetry` instantiation წაშლილია. კლასი (`telemetrymanager.h`) რჩება opt-in სამომავლო გამოყენებისთვის.
  ⚠️ ~~dead references ჯერ კიდევ რჩება~~ → გასუფთავდა: forward decl, member, include, CMakeLists — ყველა წაშლილია.

- [x] **mainwindow.cpp: dead-object stubs — nullptr parent = მეხსიერების გაჟონვა**
  13 ობიექტი instantiate ხდებოდა `nullptr` parent-ით, არასდროს გამოიყენებოდა, არასდროს იშლებოდა.
  → ყველა წაშლილია. member-ები header-ში რჩება `= nullptr`.

- [x] **ChatPanelWidget — slash command autocomplete ცარიელი**
  `setSlashCommands()` არასდროს გამოიძახებოდა ChatPanelWidget-დან → autocomplete popup-ი მუდამ ცარიელი იყო.
  → გამოსწორდა: `buildUi()`-ში 11 slash command-ი დარეგისტრირდა.

- [x] **chatpanelwidget.cpp: `/test` vs `/tests` შეუსაბამობა**
  welcome chips-ი `"/tests"`-ს აჩვენებდა, resolver `/test`-ს პარსავდა → `mid(5)` "s ..." ლიდინგ სიმბოლოს ტოვებდა.
  → გამოსწორდა: resolver იყენებს `/tests` + `mid(6)`. prefix order fixed (longer first).

- [x] **ChatWelcomeWidget — suggestion chip display/command გამიჯვნა**
  chips-ი აჩვენებდა "/explain Explain this codebase" (slash command + label ერთად).
  → გამოსწორდა: `Suggestion { label, command }` struct. Button-ი label-ს აჩვენებს, command-ს აგზავნის.

- [x] **resolveSlashCommand — დამატებითი commands**
  `vscode-copilot-chat`-ის reference-ის მიხედვით დაემატა: `/generate`, `/search`, `/compact`.
  `AgentIntent::GenerateCode` enum value-ც დაემატა `aiinterface.h`-ში.

- [x] **ChatTheme — light theme infrastructure**
  ყველა dark token-ისთვის `L_` prefixed light variant დაემატა.
  `isDark()` + `pick(dark, light)` runtime helpers.
  `ChatTranscriptView` + `ChatPanelWidget` ახლა `pick()` იყენებს background-ებისთვის.

- [x] **Chat UI პოლირება — ვიზუალური თანმიმდევრულობა**
  - Tool card `updateState()` — `border-radius` და `padding` კონსტრუქტორთან სინქრონიზებულია (4px/8px 10px)
  - Tool confirmation — `m_allowMenuBtn` re-enable `ConfirmationNeeded`-ზე
  - Welcome pills — `Qt::WA_Hover` attribute (QWidget `:hover` არ მუშაობდა)
  - ThinkingWidget — `pick()` light theme: `L_ThinkingBg`, `L_ThinkingBorder`, `L_ThinkingFg`; border-radius 2→4px
  - WorkspaceEditWidget — theme-adaptive card bg + row hover (`WA_Hover` + `pick()`)
  - SessionHistoryPopup — full `pick()` adaptation (popup bg, search box, list, context menu)
  - SettingsPanel — ChatTheme-based dark/light styling (tabs, groups, inputs, combos)

---

## მაღალი პრიორიტეტი

- [x] **TreeSitterHighlighter — ტესტები**
  ახალი highlighter-ი (3 ახალი ფაილი) zero coverage-ით. მინიმუმი:
  - byte↔char offset სისწორე (ASCII, multi-byte UTF-8)
  - incremental parse — AST-ი სწორია ჩასმის/წაშლის შემდეგ
  - `buildBlockByteOffsets()` — block count = document block count
  ფაილი: `tests/test_treesitter_highlighter.cpp` — **24 test cases, PASS**

- [x] **MainWindow God Object — partial refactor**
  **145 → 69 member** (მიზანი იყო <80 — გადავაჭარბეთ).
  Bootstrap-ები: `AgentPlatformBootstrap`, `LspBootstrap`, `BuildDebugBootstrap`, `StatusBarManager`, `AIServicesBootstrap`.
  Dock pointer removal: 19 `ExDockWidget*` → `dock()` helper.
  Dead code cleanup: 16 never-instantiated member declarations removed.

- [x] **OAuthManager — `socket->flush()` Qt6 compatibility**
  `socket->flush()` Qt6-ში წაშლილია → `waitForBytesWritten(0)`.
  → გამოსწორდა (commit `02c5a70`).

- [x] **OAuthManager — Token exchange stub**
  ✅ სრულად იმპლემენტირებულია: `exchangeCodeForToken()` — HTTP POST GitHub-ის token endpoint-ზე,
  PKCE code_verifier, JSON parsing, error handling, `loginSucceeded`/`loginFailed` სიგნალები.
  ფაილი: `src/agent/oauthmanager.h`

---

## საშუალო პრიორიტეტი

- [x] **outputpanel.cpp: `QDir::currentPath()` fallback**
  Lines 271, 312 — fallback ამოღებულია, `m_workDir` პირდაპირ გამოიყენება.

- [x] **TelemetryManager dead references გასუფთავება**
  instantiation წაშლილია, dead references გასუფთავდა:
  - `mainwindow.h` — forward declaration + member წაშლილია
  - `mainwindow.cpp` — include წაშლილია
  - `CMakeLists.txt` — header listing წაშლილია

- [x] **onContentsChange: row/col accuracy**
  ✅ `byteToPoint()` ოპტიმიზებულია. `buildBlockByteOffsets()` pre-computes per-block byte positions.
  `charToByteOffset()`/`byteToCharOffset()` tight O(n) loops. კოდში კომენტარი: "avoids toPlainText() on every block".

- [x] **PieceTableBuffer — T2 upgrade**
  ✅ `PieceTableBuffer::text()` არსებობს და ოპტიმიზებულია (`slice(0, m_length)`).
  `toPlainText()` calls არის QPlainTextEdit-ზე (diffviewer, proposededit) — PieceTableBuffer-ზე არა. არაფერია შესაცვლელი.

---

## დოკუმენტაცია / Roadmap

- [x] **docs/roadmap.md — Phase 11**
  Phase 11 (Self-Hosting Dogfood) დაწერილია roadmap-ში.
  Pre-dogfood cleanup ✅, capability matrix ✅, remaining tasks ✅.

---

## არქიტექტურული ვალი (გრძელვადიანი)

- [x] **SDK layer — plugin isolation**
  ✅ SDK boundary სრულად დაცულია. ყველა plugin ინტერფეისებს იყენებს:
  `agent/iagentplugin.h`, `plugininterface.h`, `aiinterface.h`. არცერთი plugin არ `#include`-ებს `mainwindow.h`-ს.
  `src/sdk/hostservices.h` — 9 ინტერფეისი სრულად იმპლემენტირებული.

- [x] **ServiceRegistry სრული გამოყენება**
  ✅ 14 სერვისი რეგისტრირებულია 3 bootstrap point-ში:
  - `mainwindow.cpp`: mainwindow, agentOrchestrator, secureKeyStorage
  - `bridgebootstrap.cpp`: processManager, bridgeClient
  - `agentplatformbootstrap.cpp`: toolRegistry, contextBuilder, agentController, sessionStore + 5 სხვა
  God-object pattern აღარ არის — სერვისები განაწილებულია bootstrap-ებში.

- [x] **Test coverage გაფართოება**
  **63 test targets, 63/63 pass** (2026-03-20).
  ახალი ტესტები: lspclient, searchworker, outputpanel, toolregistry, contextbuilder, gitservice, multicursor, projectmanager, terminalscreen, treesitter_highlighter, testdiscovery, problemspanel + many more.

---

## Phase 12 — Core Philosophy Alignment

> წყარო: [docs/core-philosophy.md](../docs/core-philosophy.md)
> აუდიტი: [docs/core-philosophy-alignment-audit.md](../docs/core-philosophy-alignment-audit.md)

### Phase A: Zero-Dependency Extractions

- [x] **A1. GitHub integration plugin-ად გამოტანა** ✅
  `src/github/` → `plugins/github/` — GhService, GhPanel as GitHubPlugin (IPlugin + IViewContributor).
  GhBootstrap removed. MainWindow shrunk: 5 `findChild<GhBootstrap*>` calls removed, include removed.
  Plugin.json manifest declares GitHubDock view. ContributionRegistry handles dock creation.

- [x] **A2. Remote/SSH plugin-ად გამოტანა** ✅
  `src/remote/` → `plugins/remote/` — SshSession, SshConnectionManager, RemoteFilePanel, RemoteSyncService as RemotePlugin.
  MainWindow shrunk: 3 members removed (m_sshManager, m_remotePanel, m_syncService), 3 forward declarations removed,
  5 includes removed, View menu toggle removed, 50+ lines of signal wiring removed.
  RemoteLspManager excluded from plugin (depends on core LSP SocketLspTransport) — stays deferred.
  Binary size: 184 MB → 179 MB (-5 MB).

### Phase B: Infrastructure Prerequisites

- [x] **B1. IBuildSystem SDK interface** ✅
  `src/sdk/ibuildsystem.h` — configure(), build(), clean(), targets(), buildDirectory().
  BuildSystemService adapter wraps CMakeIntegration. Registered as "buildSystem" in ServiceRegistry.

- [x] **B2. ITestRunner SDK interface** ✅
  `src/sdk/itestrunner.h` — discoverTests(), runAllTests(), runTest(), tests(), hasTests().
  TestRunnerService adapter wraps TestDiscoveryService. TestItem struct moved to SDK. Registered as "testRunner".

- [x] **B3. BuildDebugBootstrap გაყოფა** ✅
  `BuildBootstrap` (toolchain, cmake, toolbar, output) + `DebugBootstrap` (debug launcher, GDB adapter, debug panel).
  BuildDebugBootstrap now compatibility wrapper delegating to both sub-bootstraps.
  MainWindow unchanged — uses same BuildDebugBootstrap interface.

- [x] **B4. Agent callback model refactor** ✅
  3 critical agent callbacks (buildProjectFn, runTestsFn, buildTargetsGetter) refactored
  from direct m_cmakeIntegration to ServiceRegistry IBuildSystem lookups.
  Graceful nullptr handling when service unavailable.

- [x] **B5. Plugin activation model implementation** ✅
  PluginManager lazy loading: shouldDeferPlugin() splits immediate vs deferred.
  activateByEvent(event) for command/view triggers. activateByWorkspace(root) for
  workspaceContains pattern matching on folder open. DeferredPlugin tracking.

- [x] **B6. Contribution interface wiring** ✅
  ContributionRegistry now registers languages, tasks, settings, themes from manifests.
  Query APIs: languageForExtension(), languageById(), registeredTasks(), taskContributor(),
  registeredSettings(), settingsForPlugin(), notifySettingChanged(), registeredThemes().
  Language/task/settings contributors resolved via dynamic_cast on plugin instance.
  Settings defaults persisted to QSettings on registration. Cleanup on unregisterPlugin().
  16 test cases in test_contributionregistry.cpp.

### Phase C: Subsystem Extractions

- [x] **C1. Build system plugin-ად გამოტანა** ✅
  `src/build/` → `plugins/build/` — CMakeIntegration, ToolchainManager, BuildToolbar, DebugLaunchController, BuildSystemService.
  OutputPanel და RunLaunchPanel რჩებიან `src/build/`-ში როგორც shared UI types.
  ILaunchService SDK interface added. DebugBootstrap simplified (debug-only).
  IHostServices extended with registerService/queryService.
  MainWindow build-free: 7 member variables removed, services via ServiceRegistry.
  Exe exports symbols (--export-all-symbols) for plugin linking.
  Project detection activation: CMakeLists.txt, Cargo.toml, Makefile, build.gradle, meson.build.

- [x] **C2. Testing system plugin-ად გამოტანა** ✅
  `plugins/testing/` — TestDiscoveryService, TestExplorerPanel, TestRunnerService.
  activationEvents: workspaceContains:CTestTestfile.cmake, CMakeLists.txt.
  ITestRunner ("testRunner") registered via ServiceRegistry. wireBuildSystem() connects IBuildSystem.
  MainWindow clean: no testing members. Qt test output via -o file.txt,txt (Windows GUI subsystem fix).

- [x] **C3. Debug system plugin-ად გამოტანა** ✅
  `plugins/debug/` — GdbMiAdapter, DebugPanel, DebugServiceBridge as DebugPlugin.
  activationEvents: workspaceContains:CMakeLists.txt, Makefile, Cargo.toml.
  IDebugService ("debugService") + IDebugAdapter ("debugAdapter") via ServiceRegistry.
  Commands: debug.launch, continue, stepOver, stepInto, stepOut, terminate. Full toolbar + menu.

- [x] **C4. Clangd Manager plugin-ად გამოტანა** ✅
  `plugins/cpp-language/` — ClangdManager, LspClient, LspServiceBridge, CppWorkspacePanel.
  CppLanguagePlugin owns its own clangd process + LSP stack.
  LspBootstrap (src/bootstrap/) is dead code — not compiled in src/CMakeLists.txt.
  ILspService registered; 30+ commands; full build/debug/test/search service wiring.

- [x] **C5. Language highlighting data plugin-ად გამოტანა** ✅
  `plugins/lang-pack/` — LangPackPlugin registers 40+ languages via SyntaxHighlighter::registerLanguage().
  SyntaxHighlighter now uses a contributor registry (extension/filename/shebang lookup); hard-coded maps removed.
  src/editor/languages/*.cpp compiled ONLY by lang-pack plugin (removed from src/CMakeLists.txt).
  SyntaxHighlighter::LanguageContribution struct + LangBuildFn type added to syntaxhighlighter.h.
  Tests use lang_test_register.h helper (registerAllTestLanguages()) to seed the registry.

### Phase D: God Object Decomposition

- [x] **D1. EditorManager extraction** ✅
  `src/editor/editormanager.cpp/h` — openFile(), closeTab(), closeAllTabs(), closeOtherTabs() moved from MainWindow.
  Deps: setFileSystem(), setCentralStack(), setLanguageProfileManager() — set during MainWindow init.
  Signals: editorOpened(EditorView*, QString) for per-editor wiring (LSP, AI, breakpoints); statusMessage(); tabClosed().
  MainWindow::openFile() is now a 1-line delegate. showTabContextMenu() uses EditorManager methods.

- [x] **D2. DockBootstrap extraction** ✅
  `src/bootstrap/dockbootstrap.cpp/h` — 14 dock panels created via DockBootstrap::initialize().
  Deps struct: parent, services, dockManager, themeManager, bridgeClient, orchestrator, hostServices, memoryPath.
  Panel accessors: chatPanel(), symbolPanel(), referencesPanel(), requestLogPanel(), trajectoryPanel(),
  settingsPanel(), memoryBrowser(), themeGallery(), diffViewer(), proposedEditPanel(), mcpClient(), mcpPanel(), pluginGallery().
  MainWindow assigns these via m_dockBootstrap->xxx() after initialization.

- [x] **D3. Target: MainWindow <40 members** ✅
  Current: **39 members** (below target of 40).
  Members breakdown: 4 managers, 3 bootstrap pointers, 3 core ptrs, 13 panel aliases from DockBootstrap,
  4 inline/chat widgets, 5 service/registry pointers, 3 layout widgets, 4 misc.

### Phase E: Plugin Ecosystem Polish

- [x] **E1. C++ plugin manifests** ✅
  copilot, claude, codex, ollama, byok — all have plugin.json with activationEvents: ["*"],
  requestedPermissions: ["NetworkAccess"], and settings contributions (enabled flag per provider).

- [x] **E2. Plugin Gallery enable/disable** ✅
  PluginGalleryPanel::onInstalledItemClicked() shows Enable/Disable toggle button.
  PluginManager::setPluginDisabled()/isPluginDisabled() persist state.
  Disabled items shown greyed-out with "(disabled)" suffix in list.

- [x] **E3. Agent tool plugin wiring** ✅
  IAgentToolPlugin discovery loop fixed: was gated behind IAgentPlugin check — now any plugin can implement it.
  GitPlugin implements IAgentToolPlugin::createTools() → returns GitOpsTool with QPointer<GitService> executor.
  gitExecutor callback removed from mainwindow.cpp Callbacks struct (~200 lines removed from mainwindow).
  git_ops tool now owned by the Git plugin, not AgentPlatformBootstrap.

### Chat & Streaming UX

- [x] **Text selection/copy** — Ultralight chat panel-ში ტექსტის მონიშვნა და კოპირება: added `::selection` CSS rule so highlights are visible; Ctrl+C and drag-to-select already wired through UltralightWidget + ChatJSBridge
- [x] **Stream consolidation** — tool call-ები compact unified stream-ად (VS Code parity): wk-box groups tool calls per cycle, auto-collapses on finish; added streaming-step shimmer + spinning icon CSS; fixed wk-box reuse across reasoning cycles
- [x] **Terminal tool hang fix** — replaced blocking `waitForFinished()` with `QEventLoop`, added `cancelForeground()` + `RunCommandTool::cancel()`
