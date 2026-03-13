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
