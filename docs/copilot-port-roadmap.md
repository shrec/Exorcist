# Copilot Qt Port — Detailed Implementation Roadmap

> **მიზანი:** VS Code Copilot Chat extension-ის ფუნქციონალი 1:1 Qt/C++17-ში,
> Electron/Node.js-ის გარეშე.
>
> **ბაზა:** `plugins/copilot/` + `src/agent/` + `src/agent/chat/`
>
> **ლეგენდა:** ✅ done · 🔧 stub/partial · ❌ missing

---

## Phase 1 — Stability & Security

### 1.1 SecureKeyStorage → CopilotProvider wiring

**სტატუსი:** `SecureKeyStorage` კლასი განსაზღვრულია (`src/agent/securekeystorage.h`),
მაგრამ `CopilotProvider` კვლავ იყენებს `QSettings` (plaintext).

**ფაილები:**
- `plugins/copilot/copilotprovider.cpp`
- `src/agent/securekeystorage.cpp` (Windows DPAPI impl შეამოწმე/დაასრულე)

**ნაბიჯები:**
1. `securekeystorage.cpp`-ში Windows impl:
   ```cpp
   // WinCred.h + CredWrite / CredRead / CredDelete
   bool SecureKeyStorage::storeKey(const QString &service, const QString &key) {
       CREDENTIALW cred = {};
       cred.Type = CREDENTIAL_TYPE_GENERIC;
       cred.TargetName = /* credentialTarget(service).toStdWString() */;
       cred.CredentialBlobSize = key.toUtf8().size();
       cred.CredentialBlob = (LPBYTE)key.toUtf8().data();
       cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
       return CredWriteW(&cred, 0);
   }
   ```
2. `copilotprovider.cpp`-ში ყველა `QSettings` call-ი ჩაანაცვლე:
   - `settings.setValue("copilot/githubToken", m_githubToken)` →
     `m_keyStorage->storeKey("exorcist/copilot/githubToken", m_githubToken)`
   - `settings.value("copilot/githubToken")` →
     `m_keyStorage->retrieveKey("exorcist/copilot/githubToken")`
3. `CopilotProvider`-ში `SecureKeyStorage *m_keyStorage` member დაამატე.
4. `clearSavedAuth()`-ში `m_keyStorage->deleteKey(...)` დაამატე.
5. Linux-ზე libsecret fallback (გამოიყენე `qt-keychain` ბიბლიოთეკა
   CMAKE-ში `find_package(qtkeychain)` — MIT ლიცენზია).

**ტესტი:** ახალი auth → restart → token კვლავ valid.

---

### 1.2 Rate Limit Header Parsing

**სტატუსი:** ❌ მთლიანად აკლია.

**ფაილები:**
- `plugins/copilot/copilotprovider.cpp` → `retryOrFail()` + `connectReply()`

**ნაბიჯები:**
1. `connectReply()`-ში reply headers-ს წაიკითხე როცა მოდის:
   ```cpp
   connect(reply, &QNetworkReply::finished, this, [this, reply]() {
       int remaining = reply->rawHeader("x-ratelimit-remaining").toInt();
       int resetTs   = reply->rawHeader("x-ratelimit-reset").toInt();
       int retryAfter = reply->rawHeader("retry-after").toInt();
       // შენახე state-ში
   });
   ```
2. სტრუქტურა დაამატე `CopilotProvider`-ში:
   ```cpp
   struct RateLimitState {
       int  remaining   = -1;   // -1 = unknown
       int  resetEpoch  = 0;
       bool throttled   = false;
   } m_rateLimit;
   ```
3. 429 status-ზე `retryOrFail()`-ში:
   - გამოთვალე `retryAfterMs` = max(retryAfter * 1000, resetEpoch - now())
   - `QTimer::singleShot(retryAfterMs, ...)` vs გაგზავნე retry
   - emit `rateLimitHit(int secondsUntilReset)` signal
4. `AgentChatPanel`-ში დაუკავშირე signal → toast notification
   (`NotificationToast` უკვე არსებობს `src/ui/`-ში).
5. `NetworkMonitor` (სრულად impl-ია header-ში) → wire MainWindow-ში:
   - `m_networkMonitor->start()` on init
   - `wentOffline()` → toast + disable send button

---

### 1.3 Async Context Building ✅

**სტატუსი:** ✅ implemented — `buildContextAsync()` via `QtConcurrent::run` + `QFutureWatcher`.

**ფაილები:**
- `src/agent/contextbuilder.h` / `.cpp`
- `src/agent/agentcontroller.cpp` → `sendModelRequest()`

**ნაბიჯები:**
1. `ContextBuilder`-ში async variant დაამატე:
   ```cpp
   QFuture<ContextSnapshot> buildContextAsync(
       const QString &userPrompt,
       const QString &activeFilePath,
       const QString &selectedText,
       const QString &languageId);
   ```
2. impl: `QtConcurrent::run([this, ...]{ return buildContext(...); })`
3. `AgentController::sendModelRequest()`-ში:
   ```cpp
   auto watcher = new QFutureWatcher<ContextSnapshot>(this);
   connect(watcher, &QFutureWatcher<ContextSnapshot>::finished, this, [=]{
       ContextSnapshot ctx = watcher->result();
       // build request & send
       watcher->deleteLater();
   });
   watcher->setFuture(m_contextProvider->buildContextAsync(...));
   ```
4. `CMakeLists.txt`-ში: `find_package(Qt6 ... Concurrent)` + link `Qt6::Concurrent`.

---

### 1.4 Context Window Token Budget ✅

**სტატუსი:** ✅ implemented — UTF-8 byte-based token estimation, `setMaxTokenBudget()`, pruner integrated.

**ფაილები:**
- `src/agent/contextbuilder.cpp`
- `src/agent/contextpruner.cpp` (impl)

**ნაბიჯები:**
1. `ContextPruner::prune()` impl:
   ```cpp
   QList<ContextItem> ContextPruner::prune() const {
       auto sorted = m_items;
       std::stable_sort(sorted.begin(), sorted.end(),
           [](const ContextItem &a, const ContextItem &b){
               if (a.pinned != b.pinned) return a.pinned > b.pinned;
               return a.priority < b.priority; // lower = higher priority
           });
       QList<ContextItem> result;
       int total = 0;
       for (auto &item : sorted) {
           if (item.pinned || total + item.tokens <= m_maxTokens) {
               result.append(item);
               total += item.tokens;
           }
       }
       return result;
   }
   ```
2. `ContextBuilder::buildContext()`-ში `ContextPruner` გამოიყენე
   `pruneContext()` static method-ის ნაცვლად.
3. Token count: `content.toUtf8().size() / 4` (bytes more accurate than chars).
4. მოდელების `inputTokenLimit`-ი (უკვე არის `ModelInfo::inputTokenLimit`)
   → `m_maxContextChars = model.inputTokenLimit * 4`.

---

## Phase 2 — Chat UI Polish

### 2.1 Code Block Syntax Highlighting ✅

**სტატუსი:** ✅ implemented — `highlightCode()` scanner in `markdownrenderer.cpp`, 20+ languages, VS Code Dark Modern colors.

**ფაილები:**
- `src/agent/markdownrenderer.cpp`
- `src/agent/chat/chatmarkdownwidget.cpp`

**ნაბიჯები:**
1. `MarkdownRenderer::renderCodeBlock(lang, code)` method დაამატე.
2. Language → SyntaxHighlighter map:
   ```cpp
   // src/editor/syntaxhighlighter.cpp უკვე მხარდაჭერს:
   // cpp, python, javascript, rust, json, xml, sql, cmake, markdown...
   static QMap<QString, SyntaxHighlighter::Language> kLangMap = {
       {"cpp", SyntaxHighlighter::Language::Cpp},
       {"python", SyntaxHighlighter::Language::Python},
       ...
   };
   ```
3. Code block render flow:
   - `QTextDocument` შექმენი code-ისთვის
   - `SyntaxHighlighter` მიაბე
   - `document->toHtml()` გამოიყენე code block-ის HTML-ად
4. Alternative (simpler): გამოიყენე CSS `<pre><code class="language-cpp">` +
   inline highlight via regex (სინტაქსური keywords).
5. `ChatMarkdownWidget`-ში `<pre>` block-ები replace with highlighted HTML.

---

### 2.2 Code Block Actions (Copy / Insert at Cursor) ✅

**სტატუსი:** ✅ implemented — Apply/Insert/Copy/Run action links in code blocks, `handleAnchorClick()` handler.

**ფაილები:**
- `src/agent/chat/chatmarkdownwidget.cpp`
- `src/agent/chat/chatmarkdownwidget.h`

**ნაბიჯები:**
1. Code block-ის HTML-ში inject floating toolbar:
   ```html
   <div class="code-block">
     <div class="code-toolbar">
       <button onclick="copyCode(0)">Copy</button>
       <button onclick="insertCode(0)">Insert</button>
     </div>
     <pre><code>...</code></pre>
   </div>
   ```
2. `QTextBrowser` → `QWebEngineView` migration საჭირო იქნება JS-ისთვის.
   ან: `ChatMarkdownWidget`-ში custom `QWidget` overlay-ი code blocks-ის
   position-ზე (geometry tracking).
3. **Simpler approach:** `ChatMarkdownWidget`-ში `contextMenuEvent`-ზე
   "Copy code block" action → `QClipboard::setText(extractedCode)`.
4. signal: `codeInsertRequested(const QString &code, const QString &lang)` →
   `AgentChatPanel` → `MainWindow` → `EditorView::insertAtCursor()`.

---

### 2.3 @Reference Badges in Messages ✅

**სტატუსი:** ✅ implemented — `TurnReference` struct, badge rendering, population from `ContextSnapshot`.

**ფაილები:**
- `src/agent/chat/chatturnwidget.cpp`
- `src/agent/chat/chatcontentpart.h`
- `src/agent/chat/chatturnmodel.h`

**ნაბიჯები:**
1. `ChatTurnModel`-ში `references` field დაამატე:
   ```cpp
   struct TurnReference {
       QString label;   // "@workspace", "@file:main.cpp", "@terminal"
       QString tooltip; // full path or content preview
   };
   QList<TurnReference> references;
   ```
2. `AgentController`-ში turn-ის populate-ისას `ContextSnapshot::items`-დან
   references ამოიღე.
3. `ChatTurnWidget`-ში references bar:
   ```
   ┌─────────────────────────────────┐
   │ [📁 workspace] [📄 main.cpp:42] │  ← clickable badges
   └─────────────────────────────────┘
   ```
   `QLabel` with `setOpenExternalLinks(false)` + `linkActivated` signal.
4. Click → `MainWindow::openFileAtLine(path, line)`.

---

### 2.4 Tool Execution Progress Visualization

**სტატუსი:** `ChatToolInvocationWidget` exists, მაგრამ streaming progress არ აქვს.

**ფაილები:**
- `src/agent/chat/chattoolinvocationwidget.cpp`
- `src/agent/agentcontroller.cpp`

**ნაბიჯები:**
1. `AgentController`-ში `toolCallStarted` signal-ი უკვე emit-ს — wire it:
   ```cpp
   connect(controller, &AgentController::toolCallStarted,
           transcriptView, &ChatTranscriptView::onToolStarted);
   ```
2. `ChatToolInvocationWidget`-ში state machine:
   - `Queued` → spinner + "Running read_file…"
   - `Streaming` → progress bar (indeterminate)
   - `CompleteSuccess` → checkmark + duration
   - `CompleteError` → ❌ + error text
3. `QMovie` ან `QTimer`-based spinner animation.
4. `toolCallFinished` signal-ი → update widget state.

---

### 2.5 Confirmation Dialog (Dangerous Tool Approval) ✅

**სტატუსი:** ✅ implemented — `ToolApprovalService` centralized, wired to `AgentController`, inline confirmation in chat.

**ფაილები:**
- `src/agent/toolapprovalservice.cpp`
- `src/agent/chat/chatturnwidget.cpp`

**ნაბიჯები:**
1. Confirmation widget (**inline** in chat, VS Code-ის მსგავსად):
   ```
   ┌──────────────────────────────────────────┐
   │ ⚠ run_command wants to execute:          │
   │   git reset --hard HEAD                  │
   │                                          │
   │  [Allow Once]  [Allow Always]  [Deny]    │
   └──────────────────────────────────────────┘
   ```
2. `ToolApprovalService::requestApproval()` → emit signal → Panel shows widget.
3. User clicks → `ToolApprovalService::resolveApproval(decision)`.
4. `AgentController` await-ს approval-ს (event loop + mutex ან Qt signal/slot).

---

## Phase 3 — Context Providers

### 3.1 ContextBuilder Callbacks — MainWindow Wiring

**სტატუსი:** `ContextBuilder` has setters for all callbacks, მაგრამ
`MainWindow`-ში გამოძახება შეუმოწმებელია.

**ფაილი:** `src/mainwindow.cpp`

**ნაბიჯები:**
1. `MainWindow::setupAgentPanel()`-ში (ან `initContextBuilder()`):
   ```cpp
   m_contextBuilder->setWorkspaceRoot(m_rootPath);
   m_contextBuilder->setFileSystem(m_fs.get());

   m_contextBuilder->setOpenFilesGetter([this]() -> QStringList {
       QStringList paths;
       for (int i = 0; i < m_tabWidget->count(); ++i)
           paths << m_tabWidget->tabToolTip(i);
       return paths;
   });

   m_contextBuilder->setGitStatusGetter([this]() -> QString {
       return m_gitService ? m_gitService->status() : QString{};
   });

   m_contextBuilder->setGitDiffGetter([this]() -> QString {
       return m_gitService ? m_gitService->diff() : QString{};
   });

   m_contextBuilder->setTerminalOutputGetter([this]() -> QString {
       return m_terminalPanel ? m_terminalPanel->lastOutput(200) : QString{};
   });

   m_contextBuilder->setDiagnosticsGetter([this]() -> QList<AgentDiagnostic> {
       return m_lspBridge ? m_lspBridge->allDiagnostics() : QList<AgentDiagnostic>{};
   });
   ```
2. `rootPathChanged` signal → `m_contextBuilder->setWorkspaceRoot(newPath)`.

---

### 3.2 @workspace Context Provider ✅

**სტატუსი:** ✅ implemented — `addWorkspaceInfoContext()` with depth-limited tree, max 300 entries.

**ფაილი:** `src/agent/contextbuilder.cpp` → `addWorkspaceInfoContext()`
ახალი method (ან `addOpenFilesContext()` გაფართოება).

**ნაბიჯები:**
1. workspace structure summary:
   ```cpp
   void ContextBuilder::addWorkspaceInfoContext(ContextSnapshot &ctx) {
       // file tree (depth 2, skip build/hidden dirs)
       QDir root(m_workspaceRoot);
       QStringList structure;
       collectTree(root, 0, 2, structure);

       ContextItem item;
       item.type = ContextItem::Type::WorkspaceInfo;
       item.label = "@workspace";
       item.content = "Project structure:\n" + structure.join("\n");
       item.priority = 10; // low priority → pruned first
       ctx.items.append(item);
   }
   ```
2. `collectTree()` helper: `QDirIterator` depth-limited, skip `build*`, `.git`,
   `node_modules`, `*.o`, `*.obj`.
3. Max 300 lines — truncate if longer.
4. `@file` reference: `ContextBuilder::addNamedFileContext(path, lineRange)` —
   called when user types `@file main.cpp:50-100` in chat input.

---

### 3.3 @git Context Provider — GitService Wiring

**სტატუსი:** `GitDiffGetter` + `GitStatusGetter` callbacks exist,
`GitService` class exists (`src/git/gitservice.h`), მაგრამ `addGitContext()`
შიდა impl შეუმოწმებელია.

**ფაილები:**
- `src/agent/contextbuilder.cpp` → `addGitContext()`
- `src/git/gitservice.cpp`

**ნაბიჯები:**
1. `addGitContext()`-ში:
   ```cpp
   void ContextBuilder::addGitContext(ContextSnapshot &ctx) {
       if (!isContextTypeEnabled(ContextItem::Type::GitStatus)) return;

       QString status = m_gitStatusGetter ? m_gitStatusGetter() : QString{};
       if (!status.isEmpty()) {
           ctx.items.append({ContextItem::Type::GitStatus, "@git status",
                             status, {}, 3, false});
       }

       QString diff = m_gitDiffGetter ? m_gitDiffGetter() : QString{};
       if (!diff.isEmpty()) {
           // Truncate large diffs
           if (diff.size() > 8000) diff = diff.left(8000) + "\n[truncated]";
           ctx.items.append({ContextItem::Type::GitDiff, "@git diff",
                             diff, {}, 4, false});
       }
   }
   ```
2. `GitService::status()` → `git status --short` QProcess output.
3. `GitService::diff()` → `git diff HEAD` output.
4. `GitService::log(int n)` → `git log --oneline -n` for recent commits.

---

### 3.4 @terminal Context Provider

**სტატუსი:** `TerminalOutputGetter` callback exists, `addTerminalContext()`
შეუმოწმებელია.

**ფაილები:**
- `src/terminal/terminalscreen.h/.cpp` — ring buffer of output
- `src/agent/contextbuilder.cpp` → `addTerminalContext()`

**ნაბიჯები:**
1. `TerminalScreen`-ში add:
   ```cpp
   /// Returns the last N lines of terminal output.
   QString lastOutput(int maxLines = 200) const;
   ```
   impl: reverse-iterate the screen buffer, collect up to `maxLines`.
2. `TerminalPanel::lastOutput(int n)` → delegates to `m_screen->lastOutput(n)`.
3. `addTerminalContext()`:
   ```cpp
   void ContextBuilder::addTerminalContext(ContextSnapshot &ctx) {
       if (!m_terminalOutputGetter) return;
       QString output = m_terminalOutputGetter();
       if (output.trimmed().isEmpty()) return;
       ctx.items.append({ContextItem::Type::Custom, "@terminal",
                         output, {}, 8, false}); // low priority
   }
   ```

---

## Phase 4 — Inline Completions & Inline Chat

### 4.1 Ghost Text — Editor Integration

**სტატუსი:** `CopilotProvider::requestCompletion()` + `completionReady()`
signal exist. `EditorView`-ში render ❌.

**ფაილები:**
- `src/editor/editorview.h/.cpp`
- `src/editor/inlinecompletionengine.h/.cpp`
- `plugins/copilot/copilotprovider.cpp`

**ნაბიჯები:**
1. `InlineCompletionEngine`-ში (stub → impl):
   ```cpp
   class InlineCompletionEngine : public QObject {
       void requestCompletion(const QString &filePath,
                              const QString &lang,
                              const QString &textBefore,
                              const QString &textAfter);
   signals:
       void suggestionReady(const QString &text);
       void suggestionCleared();
   };
   ```
2. Debounce: `QTimer` 400ms → cancel previous `QNetworkReply` → new request.
3. `EditorView`-ში:
   - `m_ghostText` member (`QString`)
   - `paintEvent()` override: after regular paint, draw ghost text in gray
     at cursor position using `painter.setPen(Qt::gray)`.
   - keyboard filter: `Tab` → `acceptGhostText()` → insert + clear.
   - `Escape` → `clearGhostText()`.
4. context: `textBefore` = last 3000 chars, `textAfter` = next 500 chars.
5. Request cancel on any keystroke (except Tab/Escape during suggestion).

---

### 4.2 Inline Chat — Editor Integration

**სტატუსი:** `InlineChatWidget` stub. Editor-ში positioning ❌.

**ფაილები:**
- `src/editor/editorview.cpp`
- `src/agent/chat/inlinechatwidget.h/.cpp` (ან `src/editor/` ში გადატანა)

**ნაბიჯები:**
1. Trigger: `Ctrl+I` → `EditorView::showInlineChat()`.
2. Widget position:
   ```cpp
   void EditorView::showInlineChat() {
       QRect cursorRect = this->cursorRect();
       QPoint pos = viewport()->mapToGlobal(cursorRect.bottomLeft());
       m_inlineChatWidget->move(pos);
       m_inlineChatWidget->show();
       m_inlineChatWidget->setFocus();
   }
   ```
3. `InlineChatWidget` minimal impl:
   - `QLineEdit` for input (with "Ask Copilot…" placeholder)
   - `Enter` → send to `AgentController` with `AgentIntent::Edit`
   - `Esc` → close
4. Response → `ProposedEditPanel` (უკვე არსებობს) → diff preview + Accept/Reject.
5. Accept → `EditorView::applyTextEdits()` (უკვე არსებობს LSP edits-ისთვის).

---

## Phase 5 — Project Brain (Persistent Memory)

> ეს ყველაზე დიდი killer feature-ია. MVP-ი შეიძლება 1-2 კვირაში.

### 5.1 .exorcist/ Directory Structure

**ახალი ფაილები შესაქმნელი:**

```
.exorcist/
  rules.json       ← workspace rules
  memory.json      ← persistent facts
  sessions/        ← session summaries (SessionStore-ი უკვე ქმნის)
  notes/
    architecture.md
    build.md
    pitfalls.md
```

---

### 5.2 ProjectBrainService ✅

**სტატუსი:** ✅ implemented — `projectbrainservice.h/.cpp`, `projectbraintypes.h`

```cpp
class ProjectBrainService : public QObject {
    Q_OBJECT
public:
    explicit ProjectBrainService(QObject *parent = nullptr);

    bool load(const QString &projectRoot);
    bool save() const;

    // Rules
    const QList<ProjectRule>& rules() const;
    void addRule(const ProjectRule &rule);
    void removeRule(const QString &id);

    // Memory facts
    const QList<MemoryFact>& facts() const;
    void addFact(const MemoryFact &fact);
    void updateFact(const MemoryFact &fact);
    void forgetFact(const QString &id);

    // Retrieve relevant subset (for prompt injection)
    QList<MemoryFact> relevantFacts(const QString &query,
                                     const QStringList &activeFiles) const;
    QList<ProjectRule> relevantRules(const QStringList &activeFiles) const;

    // Session summaries
    void addSessionSummary(const SessionSummary &summary);
    QList<SessionSummary> recentSummaries(int n = 5) const;

signals:
    void factsChanged();
    void rulesChanged();

private:
    QString   m_root;
    QList<ProjectRule>    m_rules;
    QList<MemoryFact>     m_facts;
    QList<SessionSummary> m_summaries;

    bool loadJson(const QString &path, QJsonDocument &out) const;
    bool saveJson(const QString &path, const QJsonDocument &doc) const;
};
```

**Data structs** (`src/agent/projectbraintypes.h`):
```cpp
struct ProjectRule {
    QString id, category, text, severity;
    QStringList scope; // glob patterns: ["src/crypto/**", "*.h"]
};

struct MemoryFact {
    QString id, category, key, value;
    double  confidence = 0.6; // 1.0=user, 0.9=tool_verified, 0.6=inferred
    QString source;           // user_confirmed | agent_inferred | tool_verified
    QString updatedAt;        // ISO 8601
};

struct SessionSummary {
    QString id, title, objective;
    QStringList visitedFiles, findings;
    QString result; // success | partial | failed
    QString summary;
};
```

---

### 5.3 BrainContextBuilder ✅

**სტატუსი:** ✅ implemented — `braincontextbuilder.h/.cpp`, wired into AgentController.

```cpp
struct AgentBrainContext {
    QList<ProjectRule>    relevantRules;
    QList<MemoryFact>     relevantFacts;
    QList<SessionSummary> relatedSessions;
    QString               architectureNotes;
    QString               buildNotes;
    QString               pitfallsNotes;
};

class BrainContextBuilder : public QObject {
public:
    AgentBrainContext buildForTask(
        const QString &task,
        const QStringList &activeFiles);

    // Serializes to compact text for prompt injection
    QString toPromptText(const AgentBrainContext &ctx) const;
};
```

**AgentController-ში wiring:**
```cpp
// sendModelRequest()-ში, system prompt-ის build-ამდე:
AgentBrainContext brain = m_brainBuilder->buildForTask(
    m_session->currentTurn()->userMessage(),
    {m_activeFilePath});
QString brainText = m_brainBuilder->toPromptText(brain);
// systemPrompt = brainText + "\n\n" + m_systemPrompt;
```

---

### 5.4 MemorySuggestionEngine + UI

**ნაბიჯები:**
1. Turn დასრულებისას `AgentController::finishTurn()` →
   `MemorySuggestionEngine::suggestFacts(turn, toolLog)`.
2. Suggestion output: `QList<MemoryFact>` candidates (confidence < 1.0).
3. Toast / inline banner:
   ```
   ┌─────────────────────────────────────────────────────┐
   │ 💡 Suggested memory:                                │
   │ "Changing scalar math requires running test_scalar" │
   │                          [Accept ✓]  [Edit ✎]  [✕] │
   └─────────────────────────────────────────────────────┘
   ```
4. Accept → `ProjectBrainService::addFact(fact)` with `confidence = 1.0`.

---

### 5.5 MemoryBrowserPanel UI

**სტატუსი:** `src/agent/memorybrowserpanel.h` — stub.

**impl ნაბიჯები:**
1. Widget structure: `QTabWidget` (Memory | Rules | Sessions | Notes).
2. Memory tab: `QTreeWidget` grouped by category (Architecture/Build/Tests/Pitfalls).
3. Toolbar: Add (+) / Edit (✎) / Delete (🗑) / Search (🔍).
4. Double-click → edit dialog → save → `ProjectBrainService::updateFact()`.
5. Rules tab: same pattern with scope display.
6. Notes tab: `QPlainTextEdit` → load `.exorcist/notes/*.md` → save on edit.
7. Wire to `MainWindow`: dock tab in AI panel alongside Chat.

---

## Phase 6 — Debugger Integration

> ყველაზე დიდი ნაჭერი. Milestone B-ის დასაკეტი კარი.

### 6.1 IDebugAdapter Interface

**ახალი ფაილი:** `src/debug/idebugadapter.h`

```cpp
struct DebugBreakpoint { QString file; int line; int id = -1; };
struct DebugFrame      { QString name, file; int line; int id; };
struct DebugVariable   { QString name, value, type; };

class IDebugAdapter : public QObject {
    Q_OBJECT
public:
    virtual void launch(const QString &executable,
                        const QStringList &args,
                        const QString &workingDir) = 0;
    virtual void continueExec() = 0;
    virtual void pause()        = 0;
    virtual void stepOver()     = 0;
    virtual void stepInto()     = 0;
    virtual void stepOut()      = 0;
    virtual void terminate()    = 0;

    virtual void setBreakpoint(const QString &file, int line) = 0;
    virtual void removeBreakpoint(int id) = 0;
    virtual QList<DebugFrame> callStack() = 0;
    virtual QList<DebugVariable> locals(int frameId) = 0;

signals:
    void started();
    void stopped(const QString &reason, const DebugFrame &frame);
    void breakpointHit(int id, const DebugFrame &frame);
    void terminated();
    void outputReceived(const QString &text, const QString &category);
};
```

---

### 6.2 GdbAdapter (GDB/MI Protocol)

**ახალი ფაილი:** `src/debug/gdbadapter.h/.cpp`

**ნაბიჯები:**
1. `GdbAdapter : public IDebugAdapter`
2. `QProcess m_gdb` — start `gdb --interpreter=mi --quiet`
3. MI command send: `write("-exec-run\n")`
4. MI output parser: line-by-line, prefix recognition:
   - `*stopped` → emit `stopped()`
   - `^done,bkpt=` → breakpoint confirmed
   - `~"text"` → console output
5. Key MI commands:
   ```
   -file-exec-and-symbols <exe>
   -exec-arguments <args>
   -exec-run
   -exec-continue
   -exec-next          (step over)
   -exec-step          (step into)
   -exec-finish        (step out)
   -break-insert <file>:<line>
   -break-delete <id>
   -stack-list-frames
   -stack-list-locals --simple-values
   ```
6. Windows: use `lldb-mi` or `cdb` (Windows Debugger).
   Add `EXORCIST_DEBUG_BACKEND` cmake option (gdb | lldb | cdb).

---

### 6.3 Debug Panel UI

**ახალი ფაილი:** `src/debug/debugpanel.h/.cpp`

**Sub-widgets:**
- `DebugCallStackWidget` — `QTreeWidget`, click → jump to frame
- `DebugLocalsWidget` — `QTreeWidget` (name/value/type)
- `DebugBreakpointsWidget` — `QListWidget` with enable/disable checkbox
- `DebugControlBar` — ▶ Continue | ⏸ Pause | ↷ Step Over | ↓ Step Into | ↑ Step Out | ■ Stop

**EditorView integration:**
- Breakpoint gutter: click margin → `setBreakpoint()` → red dot ●
- Current line: highlight in yellow/blue when `stopped()` signal.
- `EditorView::setDebugLine(file, line)` → open file + scroll.

---

### 6.4 Run Launch Panel

**სტატუსი:** `src/build/runlaunchpanel.h/.cpp` — exists (partially).

**ნაბიჯები:**
1. Launch config UI: executable path + args + working dir.
2. "Debug" button → `IDebugAdapter::launch(...)`.
3. Launch configs-ი persist in `.exorcist/launch.json`.
4. Recent launches history (QComboBox).

---

## Phase 7 — Session Export & Conversation Management

### 7.1 Session Export (Markdown / JSON) ✅

**სტატუსი:** ✅ implemented — `exportToMarkdown()`, `exportToJson()` in `SessionStore`.

**ფაილი:** `src/agent/sessionstore.cpp` / `src/agent/promptarchiveexporter.h` (stub).

**ნაბიჯები:**
1. `SessionStore::exportToMarkdown(sessionId, path)`:
   ```
   # Session: Optimize scalar multiply
   Date: 2026-03-08

   **User:** optimize ECC scalar multiplication

   **Assistant:** I'll analyze the scalar multiplication path...

   ```cpp
   // code block
   ```
   ```
2. `SessionStore::exportToJson(sessionId, path)` — structured JSON.
3. `ChatSessionHistoryPopup`-ში: right-click menu → "Export as Markdown" / "Export as JSON".
4. `QFileDialog::getSaveFileName()` → write file.

---

### 7.2 Auto Session Title Generation ✅

**სტატუსი:** ✅ implemented — first message truncated to 50 chars, persisted via `setSessionTitle()`.

**ნაბიჯები:**
1. First user message → first 60 chars → title candidate.
2. Or: after first model response → ask model (short prompt):
   ```
   Generate a 5-word title for this conversation: "<first_message>"
   ```
   Use lightweight request (no tools, max 20 tokens).
3. `SessionStore::setTitle(id, title)` → persist.
4. `ChatSessionHistoryPopup` shows title instead of timestamp.

---

## Implementation Priority Matrix

| Item | Impact | Effort | Priority | Status |
|------|--------|--------|----------|--------|
| 1.1 SecureKeyStorage wiring | 🔴 Security | Low | **P0** | ✅ |
| 1.2 Rate limit headers | 🔴 Stability | Low | **P0** | ✅ |
| 1.3 Async context build | 🟡 UX | Medium | **P1** | ✅ |
| 3.1 ContextBuilder wiring | 🟢 Core | Low | **P1** | ✅ |
| 2.2 Code block copy/insert | 🟢 UX | Low | **P1** | ✅ |
| 2.4 Tool progress viz | 🟢 UX | Low | **P1** | ✅ |
| 2.5 Confirmation dialog | 🟡 Core | Medium | **P1** | ✅ |
| 5.1–5.3 Project Brain MVP | 🟢 KillerFeature | Medium | **P2** | ✅ |
| 3.2 @workspace provider | 🟢 Core | Medium | **P2** | ✅ |
| 3.3 @git provider | 🟢 Core | Low | **P2** | ✅ |
| 3.4 @terminal provider | 🟢 Core | Low | **P2** | ✅ |
| 2.1 Code syntax highlight | 🟢 UX | Medium | **P2** | ✅ |
| 4.1 Ghost text | 🟢 Productivity | High | **P3** | ✅ |
| 4.2 Inline chat widget | 🟢 UX | High | **P3** | ✅ |
| 6.1–6.4 Debugger | 🔴 Milestone B | Very High | **P3** | ❌ |
| 7.1 Session export | 🟢 Polish | Low | **P4** | ✅ |
| 7.2 Auto title | 🟢 Polish | Low | **P4** | ✅ |

---

## Quick Wins (< 1 day each)

1. **Rate-limit toast** — 20 lines in `retryOrFail()` + signal + `NotificationToast` wire.
2. **NetworkMonitor wire** — `m_networkMonitor->start()` in MainWindow + offline banner.
3. **ContextBuilder callbacks** — wiring in `MainWindow::setupAgentPanel()` (30 lines).
4. **@git context impl** — `addGitContext()` calls existing `GitService` (20 lines).
5. **Code block copy** — context menu in `ChatMarkdownWidget` (15 lines).
6. **Session auto-title** — first 60 chars of message (5 lines).
