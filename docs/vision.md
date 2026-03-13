# Exorcist IDE — Vision & Strategy

სამუშაო დოკუმენტი: პროექტის ხედვა, არქიტექტურა, სტრატეგია, roadmap.

---

## 1. მისია

სწრაფი, მსუბუქი, ნეიტივ IDE, სადაც:

- Core ფუნქციები built-in
- Plugins კონტროლირებადი და context-aware
- მხოლოდ საჭირო plugins ჩაიტვირთება
- UI extensibility — HTML/JS (Ultralight)
- Automation — LuaJIT
- Heavy extensions — Native C++/Rust

**ძირითადი პრინციპები:**

| # | პრინციპი |
|---|----------|
| 1 | IDE უნდა მუშაობდეს marketplace-ის გარეშე |
| 2 | Core developer tools built-in |
| 3 | Plugins არ უნდა აუარესებდეს performance-ს |
| 4 | Installed plugins passive by default |
| 5 | UI extensibility მარტივი უნდა იყოს |

---

## 2. High-Level Architecture

```
+---------------------------------------------------+
|                   Exorcist IDE                    |
|---------------------------------------------------|
| Editor Core                                       |
|---------------------------------------------------|
| Project System | Search Engine | Indexer          |
|---------------------------------------------------|
| Plugin Manager                                    |
|---------------------------------------------------|
| Core Services                                     |
|  - Git          - Terminal                        |
|  - Debugger     - Toolchains                      |
|  - LSP (30+ languages)                            |
|---------------------------------------------------|
| AI Agent System (Tool Runtime + LuaJIT)           |
|---------------------------------------------------|
| ExoBridge Service                                 |
+---------------------------------------------------+
        |               |               |
        v               v               v
+--------------+  +-----------------+  +------------------+
| Native       |  | Hybrid Plugin   |  | Lua Automation   |
| Plugins      |  | (HTML UI)       |  | Scripts          |
+--------------+  +-----------------+  +------------------+
```

### Core Module Map

```
editor/          text engine, syntax, completion, navigation, formatting,
                 multi-cursor, piece table
project/         workspace manager, project tree, indexing, file watcher
build/           toolchain detection, build runner, cmake/cargo adapters
debug/           debugger core, GDB/MI adapter, LLDB adapter
lsp/             LSP client, transport, message framing
git/             git service, blame, diff, staging
search/          workspace search, file search, regex
terminal/        ConPTY/PTY, VT100+ emulation
remote/          SSH, remote FS, multi-arch, rsync
agent/           agent controller, tools, context, chat UI
mcp/             Model Context Protocol client
plugins/         plugin manager, loader, manifest, activation
```

---

## 3. Plugin System

### 3.1 Plugin Types

| ტიპი | ენა | UI | გამოყენება |
|-------|-----|-----|-----------|
| **Native** | C++ / Rust (C ABI) | სრული IDE ინტეგრაცია | Advanced extensions |
| **Hybrid** | C++ backend + HTML/JS UI | Ultralight panels | Tool panels, dashboards, AI UI, settings |
| **Lua** | LuaJIT | არ აქვს UI | Automation, scripting, agent helpers |

### 3.2 Plugin Classification

Plugin manifest (`plugin.json`):

```json
{
  "id": "exo.cmake.tools",
  "classification": {
    "languages": ["cpp"],
    "build": ["cmake"],
    "workflow": ["build", "debug"]
  },
  "activation": {
    "mode": "contextual"
  }
}
```

### 3.3 Activation Modes

| Mode | აღწერა |
|------|---------|
| `always` | ყოველთვის ჩატვირთული (Git, Search, Terminal, LSP) |
| `contextual` | ჩაიტვირთება workspace-ის მიხედვით (CMake → C++ project-ში) |
| `on_demand` | მხოლოდ user-ის მოთხოვნით (Notebook, Database Explorer) |
| `manual` | ხელით ჩართვა/გამორთვა |

### 3.4 Plugin States

```
Installed → Passive → Eligible → Loaded → Active
```

**მთავარი წესი: Installed ≠ Loaded**

Workspace-ის გახსნისას:

```
detect language → detect build system → detect repo type
→ activate only relevant plugins
```

### 3.5 Performance Policy

**Passive plugin-მა არ უნდა:**
- გამოყოს RAM
- გაუშვას threads
- დაასკანეროს ფაილები
- დააკვირდეს filesystem-ს
- დაარეგისტრიროს hooks

მხოლოდ metadata ჩატვირთული.

### 3.6 Workspace Profiles

```
cpp-cmake     → CMake, C++ Debug, Clang Tools
rust-cargo    → Cargo, Rust Analyzer
php-web       → PHP tools
node-web      → Node, npm, TypeScript
python-data   → Python, pip, Jupyter
```

თითო profile ააქტივებს შესაბამის plugin კლასს.

---

## 4. Hybrid Plugin Model

### არქიტექტურა

```
C++ backend + Ultralight renderer + JS bridge
```

### Template Structure

```
plugin_project/
  plugin.json
  src/
    plugin_backend.cpp
    bridge_handlers.cpp
  ui/
    index.html
    main.js
    style.css
  assets/
```

### Build Output

```
plugin.dll  (UI assets embedded)
```

### JS Plugin Framework API

```javascript
exo.emit()          // send event to backend
exo.invoke()        // call backend function
exo.onEvent()       // listen for backend events
exo.onState()       // observe state changes
exo.ready()         // signal plugin ready
```

Bridge path: `JS → Ultralight → C++ plugin backend`

---

## 5. Agent-Driven Development Environment

### 5.1 კონცეფცია

Exorcist არ არის "IDE + AI chat". ეს არის:

**IDE built around agent operability**

სამი მონაწილე:

| მხარე | როლი |
|-------|------|
| **Developer** | გადაწყვეტს, ირჩევს, ამტკიცებს |
| **IDE Core** | ფლობს state-ს, ასრულებს ოპერაციებს |
| **Agent** | აანალიზებს, გეგმავს, იძახებს tools-ს, ამოწმებს |

### 5.2 Agent Operating Cycle

```
1. Understand  →  get_editor_context, read_project, grep, semantic_search
2. Plan        →  scratchpad, todo, memory, Lua helper logic
3. Act         →  read_file, apply_patch, build, run_tests, git
4. Verify      →  get_errors, run_tests, static_analysis
5. Report      →  rich response, diffs, proposed edits, tool cards
```

### 5.3 Architecture

```
Developer
   ↓
Exorcist IDE
   ├─ Editor
   ├─ Search / Indexer
   ├─ Build / Debug
   ├─ Git
   ├─ Plugin System
   ├─ Tool Runtime (74+ tools)
   └─ Agent Runtime
          ├─ Model Provider (plugin)
          ├─ Tool Dispatcher
          ├─ Agent LuaJIT VM
          ├─ Memory / Notes / TODO
          └─ Session State
```

### 5.4 განსხვავება სხვა IDE-ებისგან

| Feature | VS Code / Cursor | Exorcist |
|---------|-----------------|----------|
| AI როლი | Autocomplete + Chat | System operator with tools |
| Tool count | 3–5 | 74+ |
| Lua sandbox | ❌ | ✅ run_lua, save/list/run_lua_tool |
| Code graph / AST | Partial | ✅ Tree-sitter AST |
| Project memory | Chat history | ✅ .exorcist persistent rules/facts |
| Change impact | ❌ | ✅ analyze_change_impact |
| Sub-agents | ❌ | ✅ orchestrated sub-agents |
| IDE introspection | ❌ | ✅ agent sees IDE state |
| Diagram gen | ❌ | ✅ Mermaid/PlantUML |
| Self-extending | ❌ | ✅ agent writes own Lua tools |

### 5.5 Agent Profiles

| Profile | როლი |
|---------|------|
| **Ask** | კითხულობს, ეძებს, ხსნის. არ ცვლის. |
| **Edit** | ამზადებს patch-ებს |
| **Build** | build/test + ანალიზი |
| **Refactor** | code graph, usages, impact analysis |
| **Research** | web/github/docs/search |
| **Qt** | moc/uic/rcc, signals/slots, Qt build issues |

### 5.6 Agent-Driven Principles

1. **Agent must not own truth** — source of truth = IDE core state
2. **Agent must use tools, not hacks** — documented APIs, audit-able
3. **Agent must be contextual** — სხვადასხვა project → სხვადასხვა tool surface
4. **Agent must be interruptible** — ქმედებები ხილული, გასაუქმებელი, დასამტკიცებელი
5. **Agent must accumulate memory** — persistent project working memory

---

## 6. LuaJIT — Agent Runtime & Tool Forge

### 6.1 LuaJIT-ის როლი

LuaJIT არ არის "კალკულატორი" — ეს არის **agent execution layer**.

```
AI Model → reasoning
LuaJIT   → lightweight execution glue
IDE Core → heavy operations
```

### 6.2 რატომ LuaJIT

| ფაქტორი | LuaJIT | Python |
|---------|--------|--------|
| Runtime size | 2–5 MB | 30–80 MB |
| Startup | instant | slow |
| FFI / native | excellent | medium |
| Sandbox | easy | hard |

### 6.3 ორი Runtime

| Runtime | მიზანი |
|---------|--------|
| **Plugin Lua** | Plugin scripting, automation, IDE commands |
| **Agent Lua** | Agent workflow, tool orchestration, helper logic |

### 6.4 Agent Tool Forge

აგენტი არ არის მხოლოდ tool user — ის არის **tool builder**.

```
User asks task
  → Agent sees tools
  → Agent realizes pattern repeats
  → Agent writes Lua tool
  → Saves it (AgentTools/)
  → Loads & reuses it later
```

### 6.5 Lua Tool Lifecycle

**Tool scopes:**

| Scope | აღწერა |
|-------|---------|
| Ephemeral | ერთჯერადი, სატესტო |
| Session | მიმდინარე სესიისთვის |
| Workspace | კონკრეტული პროექტისთვის |
| Global | ზოგადი reusable tools |

**Tool metadata:**

```json
{
  "name": "group_build_errors",
  "description": "Groups compiler errors by file and type",
  "scope": "workspace",
  "author": "agent",
  "tags": ["build", "errors", "analysis"],
  "permission": "read_only"
}
```

**Tool management API:**

| Tool | აღწერა |
|------|---------|
| `save_lua_tool` | შექმნა + სკრიპტი + metadata |
| `list_lua_tools` | ჩამონათვალი + სრული კოდი |
| `run_lua_tool` | ჩატვირთვა + შესრულება sandbox-ში |
| `delete_lua_tool` | წაშლა |
| `search_lua_tools` | ძებნა tags/name-ით |

### 6.6 Lua Tool Namespace Design (Future)

```lua
fs.*       -- filesystem operations
git.*      -- git operations
code.*     -- code analysis
build.*    -- build/test
debug.*    -- debug control
web.*      -- network/fetch
```

---

## 7. ExoBridge Service

### 7.1 რა არის

Shared background daemon — მართავს რესურსებს, რომლებიც საერთოა ყველა IDE instance-ისთვის.

### 7.2 რა ეკუთვნის ExoBridge-ს

| რესურსი | მიზეზი |
|---------|--------|
| MCP tool servers | stateless, workspace-agnostic |
| Git file watcher | per-repo, არა per-window |
| Auth token cache | ერთი login = ყველა instance |
| Package cache (future) | shared across workspaces |

### 7.3 რა რჩება per-instance

LSP, workspace index, search, terminal, editor state, build, debug sessions.

### 7.4 Startup Logic

```
IDE launch → check ExoBridge → if not running → start; else → connect
```

### 7.5 Future: ExoServer

```
ExoBridge (local) → ExoServer (distributed, multi-machine)
```

- Shared AI tool service
- Web search broker
- GitHub broker
- Cross-instance coordination

---

## 8. Qt Development Support

### 8.1 სტრატეგია

Exorcist = **modern alternative Qt development environment**
(არა "Qt Creator replacement")

**Key message:** Built with respect for Qt ecosystem.

### 8.2 Qt ლიცენზია

Qt Open Source (LGPLv3) — სრულად ლეგალურია:

✅ MIT core + Enterprise edition + Paid features + Closed plugins

**პირობა:** Dynamic linking (`Qt6Core.dll`, `Qt6Gui.dll`, etc.)

❌ არ გააკეთო static build (LGPL → GPL obligations)

### 8.3 Qt Support Roadmap

| # | Feature | აღწერა | სტატუსი |
|---|---------|---------|---------|
| 1 | **Project Detection** | `find_package(Qt6)`, `.pro`, `.ui` files | TODO |
| 2 | **Toolchain Integration** | moc, uic, rcc, qmake, cmake | TODO |
| 3 | **Qt Project Wizard** | New Qt Widgets/Console/Quick App | TODO |
| 4 | **UI Designer Support** | `.ui` preview ან external Qt Designer | TODO |
| 5 | **Signals/Slots Navigation** | Go to signal, Go to slot, Find connections | TODO |
| 6 | **Qt Class Wizard** | New QWidget, QDialog, QObject, QThread | TODO |
| 7 | **Resource System** | `.qrc` preview, edit, auto-update build | TODO |
| 8 | **Documentation** | F1 → Qt docs integration | TODO |
| 9 | **QML Support** | Syntax highlight, navigation, live preview | Future |
| 10 | **Qt Debugging** | QObject inspect, signals trace, thread view | TODO |
| 11 | **Code Intelligence** | Q_OBJECT, Q_PROPERTY, Q_SIGNAL awareness | TODO |
| 12 | **AI Qt Assistant** | Generate Qt UI, debug signals, explain errors | TODO |
| 13 | **Qt Project Analyzer** | missing moc, wrong connect, vtable errors | TODO |

### 8.4 Qt Dev Pack

Bundled plugin:

```
Qt project detection + build helpers + class wizard +
signals navigation + docs integration
```

Activation: `activate when Qt project detected`

### 8.5 რატომ გადავლენ Qt დეველოპერები

Qt Creator-ის სუსტი მხარეები:

- მძიმე, ნელი UI
- ცუდი search
- Plugin ecosystem არ არსებობს
- AI tooling არ აქვს
- Code intelligence არასრულყოფილი

Exorcist-ის უპირატესობა:

- სწრაფი startup (< 1s)
- ძლიერი search
- AI agent + Qt awareness
- Plugin სისტემა
- უკეთესი Git ინტეგრაცია

---

## 9. Performance Targets

| მეტრიკა | Target |
|----------|--------|
| Startup | < 1 წამი |
| Memory baseline | ~200 MB |
| Plugin loading | მხოლოდ context plugins |
| Idle CPU | near zero |

---

## 10. Core IDE Status (Done / Existing)

- [x] Docking system
- [x] Tabbed editor
- [x] Syntax highlighting (Tree-sitter)
- [x] IntelliSense (LSP)
- [x] Go-to definition
- [x] Project tree
- [x] Search across project
- [x] Terminal integration (ConPTY/PTY)
- [x] Toolchain detection
- [x] Debugger interface (GDB/MI)
- [x] Encoding/status bar info
- [x] Folder workspace
- [x] Remote SSH
- [x] LSP integration (30+ languages)
- [x] Multi-cursor & column selection (44 tests)
- [x] AI Agent system (74+ tools)
- [x] LuaJIT sandbox (64MB, 40 tests)
- [x] Agent Tool Forge (save/list/run Lua tools)
- [x] ExoBridge daemon (MCP, git watch, auth)
- [x] MainWindow decomposition (69 members)
- [x] 197 test cases, 12/12 targets pass

---

## 11. Development Roadmap

### Phase 1 — IDE Stabilization

- [ ] UI rendering bug fixes
- [ ] Docking polish
- [ ] Editor performance tuning
- [ ] Search engine optimization
- [ ] Indexing stability

### Phase 2 — Plugin Infrastructure

- [ ] Plugin classification system
- [ ] Activation rules engine
- [ ] Passive plugin state enforcement
- [ ] Plugin metadata loader
- [ ] Plugin Manager UI (Installed/Passive/Loaded/Disabled)

### Phase 3 — Plugin SDK + HTML Plugin Framework

- [ ] Hybrid plugin template generator (wizard)
- [ ] JS bridge API finalization
- [ ] Event system implementation
- [ ] State synchronization
- [ ] Build pipeline + resource embedding
- [ ] Plugin build → single DLL output

### Phase 4 — Qt Development Support

- [ ] Qt project detection
- [ ] CMake + Qt integration
- [ ] moc/uic/rcc support
- [ ] Qt class wizard
- [ ] Signals/slots navigation
- [ ] Qt debugging helpers

### Phase 5 — Agent Platform Expansion

- [ ] Tool library extension
- [ ] Improved sandbox
- [ ] Structured responses
- [ ] Lua tool lifecycle (scopes, metadata, review)
- [ ] Specialized agent profiles
- [ ] Agent-created tools review UI

### Phase 6 — Long-Term

- [ ] ExoServer (distributed development)
- [ ] Collaborative workspace
- [ ] Plugin marketplace
- [ ] Remote development clusters
- [ ] QML visual preview
- [ ] Qt widget inspector
- [ ] Code intelligence platform

---

## 12. Weekly Development Model (1-Month Sprint)

| კვირა | ფოკუსი | შედეგი |
|-------|--------|--------|
| **1** | IDE stabilization | სტაბილური IDE ყოველდღიური მუშაობისთვის |
| **2** | Plugin infrastructure | Installed ≠ Loaded, classification, activation |
| **3** | Plugin SDK + HTML framework | სხვებსაც შეეძლებათ plugin დაწერა |
| **4** | Qt development support | Qt developer სრულად მუშაობს Exorcist-ში |

### რატომ არის ეს რეალისტური

ბირთვი უკვე გაკეთებულია — ახლა ხდება:

```
integration + polishing
```

არა ნულიდან აშენება.

**განვითარების მოდელი:**

```
functional → stable → polished
```

---

## 13. Self-Dogfood

Exorcist-ში Exorcist-ის წერა = საუკეთესო ტესტი:

- agent სად უშლის ხელს
- სად აკლია context
- რა tool უნდა დამატდეს
- რომელი flow რთულია
- სად ჭირდება memory
- სად ჭირდება უკეთესი verification

---

## 14. Competitive Positioning

### Exorcist vs სხვები

| | VS Code | Cursor | Qt Creator | **Exorcist** |
|--|---------|--------|------------|-------------|
| Runtime | Electron | Electron | Qt/C++ | **Qt/C++** |
| Startup | 2-5s | 3-6s | 2-4s | **< 1s** |
| Memory | 500MB+ | 600MB+ | 300MB+ | **~200MB** |
| AI integration | Plugin | Core | ❌ | **Core (74+ tools)** |
| Plugin loading | All loaded | All loaded | All loaded | **Context-aware** |
| Agent tools | 3-5 | 5-10 | ❌ | **74+** |
| Self-extending AI | ❌ | ❌ | ❌ | **✅ Lua Tool Forge** |
| Native performance | ❌ | ❌ | ✅ | **✅** |

### Key Differentiator

```
Exorcist = Developer + Native IDE + Tool OS + Agent Runtime
```

არა უბრალოდ editor + AI chat.
