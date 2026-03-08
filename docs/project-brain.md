# Project Brain

Project Brain — პროექტის ცოდნის მუდმივი შენახვის სისტემა, რომელიც AI აგენტს
საშუალებას აძლევს ისწავლოს პროექტის შესახებ და გამოიყენოს ეს ცოდნა მომავალ
სესიებში. მონაცემები ინახება `.exorcist/` დირექტორიაში JSON ფორმატში.

## Files

| File | Directory | Purpose |
|------|-----------|---------|
| `projectbraintypes.h` | `src/agent/` | Shared data types (ProjectRule, MemoryFact, SessionSummary) |
| `projectbrainservice.h/.cpp` | `src/agent/` | Persistent knowledge store (CRUD + relevance queries) |
| `braincontextbuilder.h/.cpp` | `src/agent/` | Assembles brain context for prompt injection |
| `memorysuggestionengine.h/.cpp` | `src/agent/` | Auto-extracts facts from completed agent turns |
| `memorybrowserpanel.h/.cpp` | `src/agent/` | UI panel for browsing/editing brain data |

## Data Types (`projectbraintypes.h`)

### ProjectRule
Workspace-level rules that constrain AI behavior.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `QString` | Unique identifier |
| `category` | `QString` | `"style"`, `"safety"`, `"architecture"`, `"testing"` |
| `text` | `QString` | Rule content |
| `severity` | `QString` | `"must"`, `"should"`, `"may"` |
| `scope` | `QStringList` | Glob patterns — `["src/crypto/**", "*.h"]` |

### MemoryFact
Persistent facts the AI learns about the project.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `QString` | Unique identifier |
| `category` | `QString` | `"architecture"`, `"build"`, `"tests"`, `"pitfalls"`, `"conventions"` |
| `key` | `QString` | Short identifier |
| `value` | `QString` | Fact content |
| `confidence` | `double` | `1.0`=user, `0.9`=tool_verified, `0.6`=inferred |
| `source` | `QString` | `"user_confirmed"`, `"agent_inferred"`, `"tool_verified"` |
| `updatedAt` | `QString` | ISO 8601 timestamp |

### SessionSummary
Compressed summaries of past agent sessions.

| Field | Type | Description |
|-------|------|-------------|
| `id` | `QString` | Unique identifier |
| `title` | `QString` | Short session title |
| `objective` | `QString` | What the user wanted |
| `visitedFiles` | `QStringList` | Files read/modified |
| `findings` | `QStringList` | Key discoveries |
| `result` | `QString` | Outcome summary |
| `summary` | `QString` | Full prose summary |

## Storage Layout

```
<workspace>/
└── .exorcist/
    ├── rules.json        ← ProjectRule list
    ├── memory.json       ← MemoryFact list
    ├── sessions.json     ← SessionSummary list
    └── notes/
        ├── architecture.md
        ├── build.md
        └── pitfalls.md
```

All files are JSON arrays. Notes are freeform markdown files.

## ProjectBrainService

Central knowledge store. Registered as `"projectBrainService"` in `ServiceRegistry`.

### Lifecycle
```cpp
brain->load(projectRoot);  // Load from .exorcist/
brain->save();             // Persist to disk
```

### Relevance Queries
```cpp
// Rules matching active file paths (glob-based scope matching)
QList<ProjectRule> rules = brain->relevantRules(activeFiles);

// Facts matching query text or relevant to active files
QList<MemoryFact> facts = brain->relevantFacts(query, activeFiles);

// Most recent N session summaries
QList<SessionSummary> sessions = brain->recentSummaries(5);
```

### Signals
| Signal | When |
|--------|------|
| `factsChanged()` | Fact added/updated/removed |
| `rulesChanged()` | Rule added/removed |
| `summariesChanged()` | Session summary added |

## BrainContextBuilder

Assembles relevant brain knowledge into a prompt-injection context.

```cpp
auto ctx = builder->buildForTask(taskDescription, activeFiles);
QString promptSection = builder->toPromptText(ctx);
```

### Output Format
`toPromptText()` produces XML-tagged sections:

```xml
<project_rules>
- [must] No raw new/delete — use smart pointers (scope: src/**)
</project_rules>

<project_memory>
- [build] Build command: cmake --build build-llvm (confidence: 1.0)
- [architecture] MainWindow constructor is ~1400 lines (confidence: 0.9)
</project_memory>

<recent_sessions>
- "Debug subsystem implementation" — Added IDebugAdapter, GdbMiAdapter, DebugPanel
</recent_sessions>

<architecture_notes>
(contents of .exorcist/notes/architecture.md, capped at 4K chars)
</architecture_notes>
```

### Integration
`AgentController::sendModelRequest()` injects brain context as a system message
immediately after custom instructions (`.github/copilot-instructions.md`).

## MemorySuggestionEngine

Automatic fact extraction from completed agent turns.

### Flow
1. `AgentController::turnFinished` emits
2. File paths extracted from tool call JSON arguments
3. Response text gathered from all turn steps
4. `suggestFacts(userPrompt, response, touchedFiles)` called
5. Three extractors run:
   - `extractPitfalls()` — error patterns → pitfall facts
   - `extractBuildFacts()` — build/test commands mentioned
   - `extractArchitectureFacts()` — structural observations
6. Duplicates filtered against existing brain facts
7. `factSuggested(candidate)` emitted for each new fact
8. `NotificationToast::showWithAction()` shows toast with "Save" button
9. On accept: `brain->addFact(fact)` with `confidence=1.0`

## MemoryBrowserPanel

Tabbed UI panel for viewing and editing brain data.

### Tabs
| Tab | Widget | Editable | Content |
|-----|--------|----------|---------|
| Files | QTreeWidget + QPlainTextEdit | Yes | .exorcist/notes/ files |
| Facts | QTableWidget (4 columns) | CRUD | Category / Key / Value / Confidence |
| Rules | QTableWidget (4 columns) | CRUD | Category / Severity / Text / Scope |
| Sessions | QTableWidget (4 columns) | Read-only | Title / Objective / Result / Summary |

### Operations
- **Add Fact**: QInputDialog for key + value → `brain->addFact()`
- **Delete Fact**: Confirmation → `brain->forgetFact(id)`
- **Add Rule**: QInputDialog for text → `brain->addRule()`
- **Delete Rule**: Confirmation → `brain->removeRule(id)`
- **Refresh**: Auto-triggered by `factsChanged` / `rulesChanged` / `summariesChanged` signals

### Wiring
`MainWindow` calls `panel->setBrainService(brain)` after panel creation.
Panel auto-refreshes when brain data changes.
