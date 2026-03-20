# Exorcist — Claude Agent Instructions

## MANDATORY TOOL PROTOCOL (applies to ALL agents and sub-agents)

These rules override all defaults. No exceptions.

---

## TOOL 1 — SOURCE GRAPH (Code Navigation & Indexing)

**Script:** `tools/source_graph_kit/source_graph.py`
**DB:** `tools/source_graph_kit/source_graph.db`
**Rebuild:** Runs automatically after every build. Manual: `python tools/source_graph_kit/source_graph.py build`

### RULE: Query the source graph BEFORE reading any source file.

**Forbidden:**
```
Read src/mainwindow.cpp           ← WRONG without querying first
Grep "pattern" src/               ← WRONG if source graph can answer it
```

**Required:**
```bash
python tools/source_graph_kit/source_graph.py context mainwindow
python tools/source_graph_kit/source_graph.py func MainWindow::someMethod
# THEN: Read only the specific line range returned
```

### Command reference

| Goal | Command |
|------|---------|
| Understand a file or class | `context <file\|class>` |
| Find a function + line range | `func <name>` |
| Read function body (no file read!) | `body <name>` |
| Full-text search all symbols | `find <term>` |
| Find a class hierarchy | `class <name>` |
| Find a method by name | `method <name>` |
| What depends on a class | `deps <class>` |
| Compact snapshot for agents | `focus <term> [budget]` |
| Minimal dependency slice | `slice <term> [budget]` |
| Assemble bug-fix context | `bundle bugfix <term>` |
| Assemble feature context | `bundle feature <term>` |
| Project overview + stats | `summary` |
| Bottleneck/optimization queue | `bottlenecks` |
| Function call edges | `calls <term>` |
| Impact of a change | `impact <term>` |
| Log architecture decision | `decide "<text>" --why Y` |
| Query decision log | `decisions [term]` |
| Find TODO/FIXME | `todo` |
| Run raw SQL | `sql "<query>"` |

All commands run from the repo root:
```bash
python tools/source_graph_kit/source_graph.py <command> [args]
```

### Token economy rules
1. Always use `body <func>` instead of reading the full `.cpp` file when you only need one function
2. Use `func <name>` to get exact line ranges, then `Read file offset=X limit=Y`
3. Use `focus` or `slice` instead of reading multiple files — it produces a compact ranked snapshot
4. Use `bundle bugfix|feature|refactor <term>` to assemble context for complex tasks
5. Never read a file >200 lines without first calling `context` to see its structure
6. Use `find` before `Grep` when searching for a symbol name

---

## TOOL 2 — AI MEMORY (My Persistent Memory)

**Script:** `tools/ai_memory/ai_memory.py`
**DB:** `tools/ai_memory/ai_memory.db`

This is my working memory. I use it to remember decisions, patterns, blockers,
and observations across sessions so I don't repeat mistakes or redo research.

### RULE: Check memory at the START of every task.

```bash
python tools/ai_memory/ai_memory.py context <topic>   # always first
python tools/ai_memory/ai_memory.py search "<query>"  # if context misses
```

### RULE: Store decisions and observations IMMEDIATELY when I learn them.

```bash
# Key architectural decision:
python tools/ai_memory/ai_memory.py store "decision:<topic>" "<what and why>" \
    --tag decision,<subsystem>

# Bug pattern or gotcha:
python tools/ai_memory/ai_memory.py store "bug:<class>-<issue>" "<description>" \
    --tag bug,fix,<subsystem>

# Architectural fact not visible in code:
python tools/ai_memory/ai_memory.py store "arch:<topic>" "<fact>" --tag insight,arch

# Blocker for later resolution:
python tools/ai_memory/ai_memory.py store "blocker:<topic>" "<description>" \
    --tag blocker
```

### Command reference

| Goal | Command |
|------|---------|
| Recall topic (FTS + key prefix + tags) | `context <topic>` |
| Full-text search | `search "<query>"` |
| Store or update | `store "<key>" "<value>" --tag t1,t2` |
| Recall exact key | `recall "<key>"` |
| List by tag | `list --tag <tag>` |
| All tags with counts | `tags` |
| Stats | `stats` |

### Common tags
`decision`, `bug`, `fix`, `pattern`, `insight`, `arch`, `blocker`,
`qt`, `cmake`, `lsp`, `agent`, `editor`, `plugin`, `ui`, `test`, `config`

### When NOT to store
- Things already visible in query output (source graph answers those)
- Info already in `docs/` or `memory/MEMORY.md`
- Resolved debugging observations with no future value

---

## EXECUTION ORDER FOR EVERY TASK

```
1.  ai_memory context <topic>                      ← recall prior knowledge
2.  source_graph focus <term>                      ← get compact project snapshot
3.  source_graph func / body / context             ← find exact code locations
4.  Read <file> offset=X limit=Y                   ← read only what's needed
5.  [Do the work]
6.  ai_memory store <key> <value> --tag ...        ← persist new knowledge
```

Sub-agents spawned via the `Agent` tool MUST follow this same order.

---

## BUILD & REINDEX

```bash
# Build (triggers automatic source graph reindex):
cmake --build build-llvm --config Debug

# Manual reindex (incremental):
python tools/source_graph_kit/source_graph.py build -i

# Full rebuild:
python tools/source_graph_kit/source_graph.py build
```

If the graph seems stale (new files missing), run `build -i` before proceeding.

---

## Project Facts

- Build dirs: `build/` (MSVC), `build-llvm/` (Clang/Ninja, active)
- Active binary: `build-llvm/src/exorcist.exe`
- Qt6 / C++17 / Windows 10 primary target
- `src/mainwindow.cpp` — 3390 lines God Object; always use `context` before reading
- Active chat panel: `ChatPanelWidget` (NOT `AgentChatPanel` — dead code)
- 74 agent tools registered; see `memory/agent-tools.md`
- Tests: `ctest --test-dir build-llvm`
- Qt6 gotcha: `QProcess::flush()` removed → use `waitForBytesWritten(0)`
