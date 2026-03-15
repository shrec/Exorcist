# Copilot Chat — სრული პორტირების Todo სია

> **სტატუსი**: ✅ სრულად დასრულებულია — ფუნქციონალური + ვიზუალური პორტი.
>
> სამუშაო ინტეგრაციის გეგმა: `docs/copilot-chat-uiux-integration-plan.md`
> GitHub Copilot Chat v0.40.0-ის ყველა ფუნქციის ამოღება და Exorcist IDE-ში გადატანა
> ანალიზი: `ReserchRepos/copilot/src/extension/` — 35+ subdirectory, 500+ source files

---

## სტატუსი

| სიმბოლო | მნიშვნელობა |
|--------|-------------|
| ✅ | დასრულებული |
| 🔄 | მიმდინარე |
| ⬜ | არ დაწყებული |
| 🔴 | კრიტიკული / პირველი |
| 🟡 | მნიშვნელოვანი |
| 🟢 | სასარგებლო |
| ⚫ | VS Code-სპეციფიური (ადაპტაცია საჭირო) |

---

## ფაზა 0 — უკვე მიღწეული (Baseline)

- ✅ Chat panel UI (QTextBrowser + history)
- ✅ Response streaming (delta tokens)
- ✅ Session persistence (save/restore JSON)
- ✅ Tool cards (styled HTML disclosure cards)
- ✅ Multi-line auto-grow input (QPlainTextEdit 80–200px)
- ✅ Working / Analyzing animated indicator bar
- ✅ File attachment UI (⊕ chips + drag-drop)
- ✅ Context Window popup (token breakdown)
- ✅ Auth headers (Bearer token)
- ✅ AgentController + AgentRequest pipeline
- ✅ Tool system: readFile, listFiles, writeFile, fileSearch, createDir, multiReplace, insertEdit, manageTodo
- ✅ 3 agent modes (Ask, Edit, Plan) with expanded system prompts
- ✅ Environment context injection (<environment_info>)
- ✅ Session tabs (pill-style)

---

## Phase 1 — Chat UI Core 🔴

### 1.1 Chat Input კომპლექტი
- ✅ **@-mentions autocomplete** — `@file:` dropdown when typing `@` in input
- ✅ **#-file picker** — `#filename` shorthand for attaching files (VS Code `#`-reference style)
- ✅ **Slash command menu** — `/` popup dropdown: `/explain`, `/fix`, `/test`, `/doc`, `/review`, `/commit`, `/new`, `/compact`
- ✅ **Slash command execution** — each slash command resolves to proper intent + system prompt
- ✅ **Input history navigation** — ↑/↓ keys cycle through sent messages
- ✅ **Rich paste** — Ctrl+V image from clipboard → temp PNG → attachment

### 1.2 Chat History რენდერინგი
- ✅ **Markdown renderer** — CommonMark: headers, bold, italic, lists, tables, blockquotes, HR
- ✅ **Code block renderer** — code blocks with language label header bar
- ✅ **Code block actions bar** — Copy / Insert / Run links per code block
- ✅ **Reference pills** — CSS class `.ref-pill` for inline clickable file references
- ✅ **Streaming cursor** — ▋ blinking cursor at end of streaming text
- ✅ **Tool call cards** — styled HTML cards for each tool call (name, args preview, status)
- ✅ **Response feedback buttons** — 👍 / 👎 per assistant message
- ✅ **Copy full response** — 📋 Copy link per assistant message (copyresp:// URL scheme)
- ✅ **Message timestamp** — hh:mm timestamp shown next to role label
- ✅ **Retry / Regenerate** — ↻ Retry link per user message (retry:// URL scheme)
- ✅ **Edit user message** — ✎ Edit link per user message (edit:// URL scheme)
- ✅ **Stop generation** — ✕ cancel button during streaming

### 1.3 Attachment System (სრული)
- ✅ **File attachment to request** — chip files + content injected into model request
- ✅ **Selection attachment** — attachSelection() method with ✂ snippet chip
- ✅ **Image attachment** — PNG/JPEG via drag-drop or Ctrl+V paste
- ✅ **Diagnostic attachment** — attachDiagnostics() method with ⚠ chip + tooltip preview
- ✅ **Attachment preview tooltip** — hover chip → first 8 lines preview or image dimensions
- ✅ **Remove attachment** — × on chip removes it

### 1.4 Chat Header & Nav
- ✅ **Session title display** — session ID tracked in m_askSessionId
- ✅ **New chat button** — ➕ button clears session and shows welcome screen
- ✅ **Session history list** — ☰ button with QMenu dropdown of past sessions
- ✅ **Session rename** — ✏ rename action in session history menu + QInputDialog
- ✅ **Session delete** — 🗑 Delete action in session history menu with confirmation
- ✅ **Session search** — search bar at top of session history QMenu, content-based search
- ✅ **Model picker** — QComboBox with available models from active provider
- ✅ **Mode picker** — Ask / Edit / Agent pill buttons in bottom bar

---

## Phase 2 — Tool System სრული 🔴

### 2.1 გამოტოვებული ფუნდამენტური ხელსაწყოები
- ✅ **getErrors / problems tool** — GetDiagnosticsTool registered, returns compiler errors
- ✅ **getChangedFiles tool** — GetChangedFilesTool registered, returns git changed files + diffs
- ✅ **runTerminalCommand tool** — RunCommandTool registered with user approval callback
- ✅ **fetchWebPage tool** — FetchWebpageTool registered, HTTP GET
- ✅ **textSearch tool** — SearchWorkspaceTool + FileSearchTool registered
- ✅ **semanticSearch / codebase tool** — embedding-based semantic code search
- ✅ **readProjectStructure tool** — ReadProjectStructureTool registered
- ✅ **createFile tool** — WriteFileTool + CreateDirectoryTool registered
- ✅ **applyPatch tool** — ApplyPatchTool registered
- ✅ **replaceString tool** — ReplaceStringTool + MultiReplaceStringTool + InsertEditIntoFileTool registered

### 2.2 Tool Permission System
- ✅ **Tool approval dialog** — ConfirmToolFn callback → QMessageBox with Allow/Always/Deny
- ✅ **Allow once / Allow always / Deny** — three approval modes with per-session memory
- ✅ **Tool result display in UI** — styled HTML cards with tool-run/tool-ok/tool-fail CSS classes
- ✅ **Tool error handling** — tool-fail card with error message displayed

### 2.3 AgentController Tool Loop
- ✅ **Iterative tool loop** — AgentController processes tool calls iteratively until done
- ✅ **Max tool iterations limit** — configurable maxStepsPerTurn (default 20)
- ✅ **Tool result injection** — tool_result messages pushed into conversation history
- ✅ **Parallel tool calls** — QThreadPool parallel execution with QEventLoop sync

---

## Phase 3 — Inline Chat & Edits 🔴

### 3.1 Inline Chat (კოდ რედაქტორში)
- ✅ **Inline chat trigger** — Ctrl+I opens floating chat input on current selection
- ✅ **Inline chat panel** — InlineChatWidget floating QFrame anchored in editor
- ✅ **Selection context auto-attach** — selected code sent automatically via setContext()
- ✅ **Inline response rendering** — suggested edit shown as LCS diff overlay
- ✅ **Accept change** — Ctrl+Enter / Apply button applies the edit to file
- ✅ **Reject change** — Escape / Discard dismisses without changes
- ✅ **Diff view** — inline before/after LCS diff with green/red highlighting
- ✅ **Multi-chunk edits** — apply multiple hunks from one response
- ✅ **Iterate inline** — send follow-up in same inline session (widget stays open)

### 3.2 Ghost Text / Inline Completions
- ✅ **Inline completion trigger** — 500ms debounce InlineCompletionEngine
- ✅ **Ghost text rendering** — faded grey text painted after cursor in paintEvent
- ✅ **Accept completion** — Tab key accepts full ghost text
- ✅ **Partial accept** — Ctrl+→ accepts word-by-word (acceptGhostTextWord)
- ✅ **Reject completion** — Escape clears ghost text, typing dismisses
- ✅ **Manual trigger** — Alt+\ forces immediate completion request
- ✅ **Language enable/disable** — per-language completion toggle
- ✅ **Completion model selection** — choose ghost text model separately from chat model

### 3.3 Next Edit Suggestions (NES)
- ✅ **Predictive next edit** — after accepting one edit, predict next logical edit
- ✅ **Multi-file edit suggestions** — suggest edits in other files
- ✅ **Cursor jump arrow** — indicator pointing to next suggested edit location

---

## Phase 4 — Workspace Indexing & Search 🟡

### 4.1 Workspace Index
- ✅ **Local workspace index** — build local chunk/embedding index of workspace files
- ✅ **File watcher** — reindex on file change
- ✅ **Index status bar item** — progress indicator while indexing
- ✅ **Language-aware chunking** — split files at function/class boundaries
- ✅ **Ignore patterns** — respect .gitignore, configurable excludes
- ✅ **Index persistence** — save index to disk, reload on open

### 4.2 Semantic Code Search
- ✅ **Embedding generation** — generate vector embeddings for code chunks
- ✅ **Vector similarity search** — find semantically similar code for a query
- ✅ **Search result ranking** — rank by relevance score
- ✅ **Symbol search** — find functions/classes by name or description

### 4.3 Text Search (Grep-style)
- ✅ **Regex search** — full regex across all workspace files
- ✅ **Gitignore-aware** — skip ignored files
- ✅ **Line number results** — return filename:line:column matches

---

## Phase 5 — Code Review 🟡

### 5.1 Review Workflow
- ✅ **Review command** — right-click → "Review with Copilot" or `/review` slash command
- ✅ **Review selection** — review selected code block
- ✅ **Review git changes** — review staged / unstaged / all git changes
- ✅ **Review request** — send code to model with review-specific system prompt

### 5.2 Review Results UI
- ✅ **Inline review comments** — show model suggestions as gutter annotations
- ✅ **Apply suggestion** — one-click apply a review suggestion
- ✅ **Discard suggestion** — dismiss a review without applying
- ✅ **Navigate suggestions** — prev/next review suggestion
- ✅ **Thumbs up/down feedback** — rate review quality
- ✅ **Continue in chat** — move review discussion to main chat

### 5.3 Review Instructions
- ✅ **`.copilot-review-instructions.md` file support** — per-project review rules loaded automatically
- ✅ **Inline review instructions UI** — text box to specify what to look for

---

## Phase 6 — Test Generation 🟡

### 6.1 Test Generation
- ✅ **/tests slash command** — generate unit tests for selected function/class
- ✅ **Framework detection** — detect Jest, pytest, GoogleTest, Catch2, etc.
- ✅ **Test scaffold creation** — create test file next to source file
- ✅ **Custom test instructions** — `.copilot-test-instructions.md` support

### 6.2 Test Failure Fixing
- ✅ **Fix failing test** — model sees test output + source → suggests fix
- ✅ **Test output context** — include last test run output in context
- ✅ **/setupTests command** — guide user through test framework setup

---

## Phase 7 — Git Integration 🟡

### 7.1 Commit Message Generation
- ✅ **Generate commit message** — button in source control → AI generates message from staged diff
- ✅ **Custom commit instructions** — `.copilot-commit-message-instructions.md`
- ✅ **SCM input integration** — insert generated message into git commit input

### 7.2 Merge Conflict Resolution
- ✅ **Detect merge conflicts** — find `<<<<<<<` markers in files
- ✅ **Resolve with AI** — send conflict + context → get resolution suggestion
- ✅ **Apply resolution** — insert resolved code in-place

### 7.3 Git Context in Chat
- ✅ **git diff context** — GitDiffTool registered, returns unified diff
- ✅ **getChangedFiles tool** — GetChangedFilesTool + GitStatusTool + GitDiffTool registered

---

## Phase 8 — Multi-Model / BYOK / Provider 🟡

### 8.1 Model Registry
- ✅ **Model list** — fetch available models from provider
- ✅ **Model picker UI** — dropdown/dialog with model name + capability icons
- ✅ **Per-mode model** — separate model for Ask / Edit / Plan / ghost text
- ✅ **Model capability flags** — vision, tool-calling, thinking support flags

### 8.2 BYOK (Bring Your Own Key)
- ✅ **Custom endpoint** — add any OpenAI-compatible endpoint + API key
- ✅ **Custom headers** — per-provider extra HTTP headers
- ✅ **Provider list** — OpenAI, Anthropic, xAI, Gemini, Ollama, Azure OpenAI, custom
- ✅ **Secure key storage** — store API keys in OS credential store (Windows Credential Manager)
- ✅ **Endpoint validation** — test connection before saving

### 8.3 Thinking Models
- ✅ **Extended thinking support** — send `reasoning_effort` param for thinking models
- ✅ **Thinking effort control** — low/medium/high reasoning effort in AgentRequest
- ✅ **Thinking token budget** — configurable max thinking tokens
- ✅ **Show thinking** — render thinking blocks in chat (collapsible)

---

## Phase 9 — Context System სრული 🟡

### 9.1 Context Providers
- ✅ **Active editor context** — inject current file + cursor position
- ✅ **Selection context** — inject selected text
- ✅ **Open files context** — inject list of currently open files
- ✅ **Diagnostics context** — inject current build errors/warnings
- ✅ **Terminal context** — inject last N lines of terminal output
- ✅ **Git diff context** — inject current git changes

### 9.2 Context Window Management
- ✅ **Token counter** — count tokens for each context piece
- ✅ **Context popup** — show breakdown by section
- ✅ **Context pruning** — auto-remove less relevant context when near limit
- ✅ **Context priority** — user can pin/unpin context items
- ✅ **Context editing** — user can remove individual context pieces before sending

### 9.3 Custom Instruction Files
- ✅ **`.github/copilot-instructions.md`** — automatically inject as system prompt additions
- ✅ **`.copilot-codeGeneration-instructions.md`** — for code gen requests
- ✅ **`.copilot-test-instructions.md`** — for test gen requests
- ✅ **`.copilot-review-instructions.md`** — for reviews
- ✅ **`.copilot-commit-message-instructions.md`** — for commit messages

---

## Phase 10 — Memory System 🟡

### 10.1 Persistent Memory
- ✅ **Memory tool** — model can read/write memory files (`/memories/*.md`)
- ✅ **User memory** — cross-session preferences (`/memories/user.md`)
- ✅ **Session memory** — within-session notes (`/memories/session/`)
- ✅ **Repository memory** — per-project notes (`/memories/repo/`)
- ✅ **Memory file browser** — UI to view/edit memory files

### 10.2 Conversation Compaction
- ✅ **/compact command** — user can compress long conversation
- ✅ **Background auto-compaction** — automatic compression when approaching context limit
- ✅ **Compaction strategy** — summarize oldest N messages, preserving tool results

---

## Phase 11 — Prompt Files (.prompt.md) 🟢

### 11.1 Prompt File Support
- ✅ **`.prompt.md` format** — YAML frontmatter + markdown body files in `.github/prompts/`
- ✅ **Prompt file loading** — scan workspace for `.prompt.md` files
- ✅ **Prompt file picker** — `/` in chat shows prompt files in slash menu
- ✅ **Prompt file execution** — send prompt file content as user message
- ✅ **Prompt file variables** — `${workspaceFolder}`, `${file}`, `${selection}` interpolation
- ✅ **Prompt file cache** — cache loaded files, invalidate on change

---

## Phase 12 — Notebook Support 🟢

### 12.1 Jupyter Notebook Integration
- ✅ **Notebook context provider** — inject cell contents as context
- ✅ **Notebook chat participant** — specialized chat mode for notebooks
- ✅ **Cell execution tool** — agent can execute notebook cells
- ✅ **Cell output reading** — include cell outputs in context
- ✅ **Create notebook tool** — create new .ipynb from chat
- ✅ **Edit notebook cells tool** — add/modify cells via chat
- ✅ **Get notebook summary tool** — return structure (cell count, types, outputs)

---

## Phase 13 — MCP (Model Context Protocol) 🟢

### 13.1 MCP Protocol
- ✅ **MCP server connection** — connect to MCP servers via stdio or HTTP
- ✅ **MCP tool discovery** — enumerate tools from connected MCP servers
- ✅ **MCP tool execution** — call MCP tools from agent loop
- ✅ **MCP tool permissions** — same approval system as built-in tools
- ✅ **MCP server configuration** — add servers via settings JSON

### 13.2 GitHub MCP (Built-in)
- ✅ **GitHub issues tool** — search/read GitHub issues
- ✅ **GitHub PR tool** — read PR data and diffs
- ✅ **GitHub code search** — search GitHub repos
- ✅ **GitHub repo info** — repo metadata

---

## Phase 14 — Rename Suggestions 🟢

### 14.1 AI Rename
- ✅ **Auto-trigger on declaration** — when user renames variable, suggest AI name
- ✅ **Rename suggestion UI** — show suggestion inline, accept with Tab
- ✅ **Semantic rename** — AI analyzes variable usage to suggest better name
- ✅ **Language Server integration** — use LSP rename + AI suggestion

---

## Phase 15 — Web Search 🟢

### 15.1 Web Search Tool
- ✅ **fetchWebPage tool** — agent fetches URL and reads content
- ✅ **Search tool** — agent searches web query (requires search API key)
- ✅ **Domain allow/deny list** — restrict which domains agent can access
- ✅ **Citation display** — show source URL in responses

---

## Phase 16 — Developer & Debug Features 🟢

### 16.1 Debug UI
- ✅ **Request log panel** — raw JSON request/response log window
- ✅ **Context inspector** — view what context was sent with last request
- ✅ **Tool call trace** — detailed log of all tool invocations
- ✅ **Prompt archive export** — export all prompts as ZIP

### 16.2 Trajectory / Replay
- ✅ **Trajectory recording** — save multi-step agent reasoning to JSON
- ✅ **Chat replay** — replay saved trajectories step-by-step
- ✅ **Edit tracing** — track which edits made during which turn

---

## Phase 17 — Configuration & Settings UI 🟢

### 17.1 Settings Panel
- ✅ **Chat settings page** — dedicated settings page in IDE preferences
- ✅ **API key management UI** — add/remove/test API keys
- ✅ **Model configuration** — per-mode model selection UI
- ✅ **Tool enable/disable** — toggle which tools are available
- ✅ **Instruction file paths** — configure custom instruction file locations
- ✅ **Context scope** — configure what gets auto-included in context
- ✅ **Completion settings** — per-language ghost text on/off

### 17.2 Feature Flags
- ✅ **Enable/disable inline chat** — keyboard shortcut + settings toggle
- ✅ **Enable/disable ghost text** — global and per-language
- ✅ **Enable/disable review** — turn off review-related UI
- ✅ **Enable/disable memory** — turn off memory system

---

## Phase 18 — Authentication & Account 🟡

### 18.1 Auth Flow
- ✅ **Bearer token injection** — token in Authorization header
- ✅ **OAuth login flow** — open browser → GitHub OAuth → store token
- ✅ **Token refresh** — automatic refresh on 401
- ✅ **Account status indicator** — statusbar item: logged in / expired / rate limited
- ✅ **Sign-in prompt** — if not authenticated, show Sign In button in panel

### 18.2 Error States
- ✅ **Expired token UI** — show "Token expired, sign in again" message
- ✅ **Rate limit UI** — show retry countdown
- ✅ **Offline detection** — show offline message when no network

---

## Phase 19 — UI Polish & Accessibility 🟢

### 19.1 Theme Support
- ✅ **Dark/Light theme** — chat bubbles, code blocks adapt to IDE theme
- ✅ **Syntax highlight theme** — match code block highlighting to IDE theme
- ✅ **High contrast mode** — WCAG AA compliance

### 19.2 Font & Rendering
- ✅ **Monospace code font** — code blocks use IDE editor font
- ✅ **Proportional chat font** — prose uses UI font
- ✅ **Font size follows IDE settings** — respect user font size preference

### 19.3 Chat Keyboard Navigation
- ✅ **Ctrl+Shift+I** — open/focus chat panel
- ✅ **Ctrl+L** — focus chat input
- ✅ **Ctrl+I** — trigger inline chat
- ✅ **Escape** — cancel/close
- ✅ **Tab** — accept completion / suggestion

### 19.4 Misc UX
- ✅ **Empty state** — welcome message / suggested prompts when chat is empty
- ✅ **Quick chat** — floating mini chat window (Ctrl+Shift+Alt+L)
- ✅ **Status bar integration** — Copilot status icon in statusbar
- ✅ **Notification system** — non-modal notifications for completed background tasks

---

## Phase 20 — Telemetry & Privacy ⚫

### 20.1 Telemetry (Optional)
- ✅ **Event logging** — opt-in telemetry for feature usage analytics
- ✅ **Opt-out setting** — disable telemetry collection
- ✅ **Privacy policy display** — link to privacy policy in settings

### 20.2 Feedback
- ✅ **Message feedback** — 👍/👎 stored locally or sent to telemetry
- ✅ **Survey integration** — optional in-app survey prompts

---

## VS Code-სპეციფიური (ადაპტაცია საჭირო) ⚫

ეს ფუნქციები VS Code-ს API-ებზეა დამოკიდებული — Exorcist-ისთვის ადაპტაცია განსხვავდება:

| VS Code ფუნქცია | Exorcist ეკვივალენტი |
|----------------|----------------------|
| Language Model API | AgentController / IAIProvider |
| Chat Participant API | Agent modes (Ask/Edit/Plan) |
| Monaco Editor | QPlainTextEdit / QtCreator-style editor |
| Diagnostics API | CMake/compiler error parsing |
| Language Server Protocol | LSP client integration (future) |
| Task System | Build system integration |
| SCM API | Git C++ library (libgit2) |
| Extension Host | Plugin system (QPluginLoader) |
| Webview API | QTextBrowser / QWebEngineView |
| Terminal API | ITerminal interface |
| Command Palette | Keyboard shortcut + action registry |

---

## პრიორიტეტული მიმდევრობა

```
Phase 1  → Chat UI Core (slash menu, markdown, code block actions, feedback)
Phase 2  → Tool System სრული (getErrors, runTerminal, fetchWeb, textSearch)
Phase 3  → Inline Chat + Ghost Text
Phase 4  → Workspace Indexing + Semantic Search
Phase 5  → Code Review
Phase 6  → Test Generation
Phase 7  → Git Integration
Phase 8  → Multi-Model / BYOK
Phase 9  → Context System სრული
Phase 10 → Memory System
Phase 11 → .prompt.md support
Phase 12 → Notebook Support
Phase 13 → MCP Protocol
Phase 14 → Rename Suggestions
Phase 15 → Web Search
Phase 16 → Developer Debug Features
Phase 17 → Settings UI
Phase 18 → OAuth Auth Flow
Phase 19 → UI Polish
Phase 20 → Telemetry
```

---

## სტატისტიკა

- **სულ ფიჩერი**: ~280 ინდივიდუალური ფუნქცია
- **დასრულებული**: ~20 (~7%)
- **Phase 1–3 (კრიტიკული)**: ~60 ფუნქცია
- **Phase 4–10 (მნიშვნელოვანი)**: ~100 ფუნქცია
- **Phase 11–20 (სასარგებლო)**: ~100 ფუნქცია
