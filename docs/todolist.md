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
  **17 test targets, 340+ test cases, 17/17 pass.**
  ახალი ტესტები: lspclient, searchworker, outputpanel, toolregistry, contextbuilder, gitservice, multicursor, projectmanager, terminalscreen, treesitter_highlighter, testdiscovery, problemspanel.

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

- [ ] **B1. IBuildSystem SDK interface**
  `src/sdk/ibuildsystem.h` — configure(), build(), clean(), targets(), buildDirectory().
  CMakeIntegration implements IBuildSystem. ServiceRegistry-ში registration.

- [ ] **B2. ITestRunner SDK interface**
  `src/sdk/itestrunner.h` — discover(), run(), results().
  TestDiscoveryService implements ITestRunner. ServiceRegistry-ში registration.

- [ ] **B3. BuildDebugBootstrap გაყოფა**
  `BuildDebugBootstrap` → ცალკე `BuildBootstrap` + `DebugBootstrap`.
  `restartClangd()` სიგნალი → ServiceRegistry event მექანიზმით (ან signal relay service).
  DebugLauncher → optional IBuildSystem service lookup.

- [ ] **B4. Agent callback model refactor**
  45+ static callbacks → ServiceRegistry dynamic lookup.
  Agent tools check service existence at runtime — graceful absence if plugin not loaded.
  Incremental migration: ჯერ optional nullptr check ყოველ callback-ზე, შემდეგ service lookup.

- [ ] **B5. Plugin activation model implementation**
  PluginManager-ში lazy loading: `activationEvents` processing.
  Project detection: `workspaceContains` pattern matching on folder open.
  Contextual activation: deferred loading on first feature use.
  Zero-cost enforcement: verify disabled = 0 CPU/RAM.

- [ ] **B6. Contribution interface wiring**
  ILanguageContributor → HighlighterFactory integration (critical for language pack extraction).
  ITaskContributor → OutputPanel/BuildToolbar integration.
  ICompletionContributor → CompletionPopup integration.
  IFormatterContributor → LspEditorBridge integration.
  ISettingsContributor → SettingsDialog auto-generation.

### Phase C: Subsystem Extractions

- [ ] **C1. Build system plugin-ად გამოტანა**
  `src/build/` → `plugins/build/` — CMakeIntegration, ToolchainManager, BuildToolbar, DebugLaunchController, OutputPanel, RunLaunchPanel.
  Project detection activation: CMakeLists.txt, Cargo.toml, Makefile.
  IBuildSystem ServiceRegistry registration. Agent tools dynamic lookup.
  MainWindow-დან 6 member removal.

- [ ] **C2. Testing system plugin-ად გამოტანა**
  `src/testing/` → `plugins/testing/` — TestDiscoveryService, TestExplorerPanel.
  Contextual activation — test explorer გახსნაზე.
  ITestRunner ServiceRegistry registration. Depends on IBuildSystem (for buildDirectory).

- [ ] **C3. Debug system plugin-ად გამოტანა**
  `src/debug/` → `plugins/debug/` — GdbMiAdapter, DebugPanel, QuickWatchDialog, WatchTreeModel.
  Contextual activation — F5 / debug start.
  IDebugAdapter already exists. 6 agent callbacks → service dynamic lookup.
  MainWindow-დან 2 member removal.

- [ ] **C4. Clangd Manager plugin-ად გამოტანა**
  `src/lsp/clangdmanager.*` → `plugins/cpp-language/` (or language pack plugin).
  LspClient რჩება Core-ში (generic protocol). ClangdManager = C++-specific.
  Project detection: `.cpp`, `.h`, `compile_commands.json`.

- [ ] **C5. Language highlighting data plugin-ად გამოტანა**
  `src/editor/languages/` → language pack plugins via ILanguageContributor.
  ~800 ხაზი ენის მონაცემი Core-დან plugin-ებში გადატანა.
  Project detection: ფაილის გაფართოების მიხედვით.

### Phase D: God Object Decomposition

- [ ] **D1. EditorManager extraction**
  Tab/document lifecycle MainWindow-დან ცალკე კლასში.
  m_tabs, tab signals, file open/close/save logic.

- [ ] **D2. DockBootstrap extraction**
  createDockWidgets() 700+ ხაზი → ცალკე DockBootstrap კლასი.
  m_dockManager + 14 panel member → DockBootstrap.
  Lazy panel initialization support.

- [ ] **D3. Target: MainWindow <40 members**
  A+C extraction: -14 members (117→103)
  D1+D2 extraction: -24 members (103→79)
  Lazy panels: -10 members (79→~50)

### Phase E: Plugin Ecosystem Polish

- [ ] **E1. C++ plugin manifests**
  AI provider plugins-ს (copilot, claude, codex, ollama, byok) `plugin.json` დამატება.
  Declarative contributions: activation events, dependencies, settings.

- [ ] **E2. Plugin Gallery enable/disable**
  PluginGalleryPanel-ში toggle ღილაკი runtime enable/disable-თვის.
  Disabled = unloaded, 0 resources.

- [ ] **E3. Agent tool plugin wiring**
  IAgentToolPlugin::createTools() discovery and registration during plugin init.
  Plugins can contribute custom agent tools via this interface.

### Chat & Streaming UX

- [ ] **Text selection/copy** — Ultralight chat panel-ში ტექსტის მონიშვნა და კოპირება
- [ ] **Stream consolidation** — tool call-ები compact unified stream-ად (VS Code parity)
- [ ] **Terminal tool hang fix** — terminal execution stuck in "Executing" state
