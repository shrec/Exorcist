# Exorcist — სამუშაო სია

> ბოლო განახლება: 2026-03-10 (მანიფესტო აუდიტი)
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

---

## მაღალი პრიორიტეტი

- [ ] **TreeSitterHighlighter — ტესტები**
  ახალი highlighter-ი (3 ახალი ფაილი) zero coverage-ით. მინიმუმი:
  - byte↔char offset სისწორე (ASCII, multi-byte UTF-8)
  - incremental parse — AST-ი სწორია ჩასმის/წაშლის შემდეგ
  - `buildBlockByteOffsets()` — block count = document block count
  ფაილი: `tests/test_treesitter_highlighter.cpp`

- [ ] **MainWindow God Object — partial refactor**
  **129 member pointer, 3001 სტრიქონი.** კონსტრუქტორი: ~470 სტრიქონი (219→688).
  Bootstrap-ები უკვე არსებობს (`AgentPlatformBootstrap`, `LspBootstrap`, `BuildDebugBootstrap`) და გამოიყენება.
  შემდეგი ნაბიჯი: dock widget creation + signal wiring ამოღება ცალკე helper-ებში.
  **მიზანი:** კონსტრუქტორი < 300 სტრიქონი, member count < 80.

- [x] **OAuthManager — `socket->flush()` Qt6 compatibility**
  `socket->flush()` Qt6-ში წაშლილია → `waitForBytesWritten(0)`.
  → გამოსწორდა (commit `02c5a70`).

- [ ] **OAuthManager — Token exchange stub**
  Authorization code იპარსება callback-იდან, მაგრამ HTTP POST to exchange code for access token არ ხორციელდება.
  კომენტარი კოდში: "In a full implementation, exchange code for token via HTTP POST"
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

- [ ] **onContentsChange: row/col accuracy**
  `byteToPoint()` helper სწორია, მაგრამ ოპტიმიზაციისთვის byte offset table-ის გამოყენება შეიძლება (ბევრი ხაზის ფაილებზე).

- [ ] **PieceTableBuffer — T2 upgrade**
  PieceTable shadow buffer სწორად მუშაობს. ოპტიმიზაციის მომდევნო ნაბიჯი:
  EditorView-ი `buffer()->text()`-ს იყენებდეს `QTextDocument::toPlainText()`-ის ნაცვლად ძვირ ოპერაციებში.

---

## დოკუმენტაცია / Roadmap

- [x] **docs/roadmap.md — Phase 11**
  Phase 11 (Self-Hosting Dogfood) დაწერილია roadmap-ში.
  Pre-dogfood cleanup ✅, capability matrix ✅, remaining tasks ✅.

---

## არქიტექტურული ვალი (გრძელვადიანი)

- [ ] **SDK layer — plugin isolation**
  `src/sdk/hostservices.h` — ინტერფეისები და implementations **სრულყოფილია** (Command, Workspace, Editor, View, Notification, Git, Terminal, Diagnostics, Task).
  დარჩენილი საკითხი: Plugin-ებმა არ უნდა `#include "mainwindow.h"` — ამჟამად ზოგი შიდა header-ს იყენებს. SDK boundary enforcement საჭიროა.

- [ ] **ServiceRegistry სრული გამოყენება**
  ServiceRegistry არსებობს, მაგრამ სერვისების უმეტესობა MainWindow-ს member-ებია.
  Plugin SDK-ი ServiceRegistry-ს მეშვეობით ხელმისაწვდომი უნდა იყოს.

- [ ] **Test coverage გაფართოება**
  ამჟამად: 4 ფაილი (serviceregistry, sseparser, piecetable, thememanager).
  მინიმალური სამიზნე: TreeSitter highlighter, EditorView find/replace, LSP request correlation, GitService parsing.
