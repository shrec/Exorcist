# Code Graph — Codebase Intelligence Engine

SQLite-based code intelligence that indexes the entire workspace. Provides structural understanding of files, classes, functions, imports, relationships, framework-specific patterns, and a semantic metadata layer for reasoning-oriented queries.

## Architecture

```
CodeGraphIndexer (orchestrator)
  ├── ILanguageIndexer (interface)
  │     ├── CppLanguageIndexer   → C/C++, Qt, QTest
  │     ├── TsLanguageIndexer    → TypeScript, JavaScript, Jest/Mocha
  │     └── PythonLanguageIndexer → Python, pytest
  ├── CodeGraphDb               → SQLite wrapper, schema, FTS5
  └── CodeGraphQuery            → Read-only query API (JSON results)
```

### Key Types

| Type | File | Purpose |
|------|------|---------|
| `CodeGraphDb` | `codegraphdb.h/.cpp` | SQLite database wrapper — open, schema creation, FTS5 detection |
| `CodeGraphIndexer` | `codegraphindexer.h/.cpp` | Multi-step pipeline orchestrator — dispatches to language indexers and derives semantic tags |
| `CodeGraphQuery` | `codegraphquery.h/.cpp` | Query API — project summary, function lookup, file context, semantic tags (JSON) |
| `ILanguageIndexer` | `ilanguageindexer.h` | Pure virtual interface for language-specific parsing |
| `CppLanguageIndexer` | `cpplanguageindexer.h/.cpp` | C/C++ indexer (classes, includes, Qt connect/services/QTest) |
| `TsLanguageIndexer` | `tslanguageindexer.h/.cpp` | TypeScript/JS indexer (imports, classes, Jest/Mocha tests) |
| `PythonLanguageIndexer` | `pylanguageindexer.h/.cpp` | Python indexer (indent-based functions, pytest) |
| `CodeGraphUtils` | `codegraphutils.h/.cpp` | Shared utilities (`findBraceBody()` for brace-depth tracking) |

### Data Types (`codegraphtypes.h`)

| Struct | Purpose |
|--------|---------|
| `CodeGraphFunction` | Function/method with name, qualified name, class, line range |
| `CodeGraphStats` | Rebuild statistics (file/class/function/edge counts) |
| `CodeGraphClassInfo` | Class name, bases, Q_OBJECT flag, pure virtual count |
| `CodeGraphImportInfo` | Import/include path |
| `CodeGraphNamespaceInfo` | Namespace declaration |
| `CodeGraphFileMetadata` | File-level flags (has Q_OBJECT, has signals/slots) |
| `CodeGraphParseResult` | Combined parse output (classes, imports, namespaces) |
| `CodeGraphConnectionInfo` | Qt signal/slot connection (sender, signal, receiver, slot) |
| `CodeGraphServiceRef` | ServiceRegistry register/resolve call |
| `CodeGraphTestCase` | Test method (class, method, line) |
| `CodeGraphSemanticTag` | Semantic/security/performance/audit tag attached to a file, class, or function |

## ILanguageIndexer Interface

```cpp
class ILanguageIndexer {
public:
    virtual QString languageId() const = 0;
    virtual QStringList fileExtensions() const = 0;
    virtual CodeGraphFileMetadata extractFileMetadata(const QString &content) const = 0;
    virtual CodeGraphParseResult parseFile(const QString &content) const = 0;
    virtual QVector<CodeGraphFunction> extractFunctions(const QString &content) const = 0;
    virtual QString extractFirstComment(const QStringList &lines) const = 0;
    virtual QVector<CodeGraphConnectionInfo> scanConnections(const QString &content) const = 0;
    virtual QVector<CodeGraphServiceRef> scanServiceRefs(const QString &content) const = 0;
    virtual QVector<CodeGraphTestCase> scanTestCases(const QString &content) const = 0;
};
```

### Adding a New Language

1. Create `<lang>languageindexer.h/.cpp` implementing `ILanguageIndexer`
2. Add to `src/CMakeLists.txt` and `tests/CMakeLists.txt`
3. Register in `CodeGraphIndexer` constructor:
   ```cpp
   registerLanguageIndexer(std::make_shared<RustLanguageIndexer>());
   ```
4. The orchestrator auto-discovers file extensions and dispatches parsing

### Built-in Indexers

| Indexer | Languages | Extensions | Special Features |
|---------|-----------|------------|-----------------|
| `CppLanguageIndexer` | C, C++ | `.h`, `.hpp`, `.cpp`, `.c`, `.cc`, `.cxx` | Qt `connect()`, `ServiceRegistry`, `QTest`, `Q_OBJECT` detection |
| `TsLanguageIndexer` | TypeScript, JavaScript | `.ts`, `.tsx`, `.js`, `.jsx` | ES imports, `extends`/`implements`, Jest `test()`/`it()`, Mocha `describe()` |
| `PythonLanguageIndexer` | Python | `.py` | Indentation-based scope, `import`/`from`, `pytest` (`test_` prefix) |

## Database Schema

| Table | Contents |
|-------|----------|
| `files` | All indexed files with path, language, line count, subsystem |
| `classes` | Class/struct declarations with bases, interface flag, Q_OBJECT |
| `methods` | Method declarations (pure virtual, signal, slot flags) |
| `includes` | `#include` / `import` edges |
| `implementations` | Header ↔ source file pairings |
| `features` | Feature detection results (implemented/header-only/stub/missing) |
| `subsystems` | Subsystem statistics (file/line/class counts) |
| `subsystem_deps` | Inter-subsystem dependency edges |
| `class_refs` | Class cross-references |
| `namespaces` | Namespace declarations |
| `file_summaries` | One-line summary per file |
| `function_index` | Functions with exact line ranges (brace-depth tracking) |
| `edges` | Unified relationship graph (includes, implements, inherits, tests) |
| `qt_connections` | Qt signal/slot connections |
| `services` | ServiceRegistry register/resolve calls |
| `test_cases` | QTest/Jest/pytest test methods |
| `cmake_targets` | CMake build targets (executable, library, plugin, test) |
| `semantic_tags` | Derived semantic/security/performance/audit/runtime tags for files, classes, functions |
| `fts_index` | FTS5 full-text search index |

## Semantic Tag Layer

The graph now derives first-class semantic tags with four practical reasoning layers:

- `semantic:*` for domain/workbench role classification such as `semantic:build`, `semantic:ui`, `semantic:analysis`
- `security:*` for sensitive surfaces such as `security:auth-surface`, `security:sensitive`, `security:fragile`
- `performance:*` for reasoning hints such as `performance:async`, `performance:hot-path`
- `audit:*` for lightweight coverage hints such as `audit:unit-covered`, `audit:uncovered`

Tags are attached to:

- files
- classes
- functions

Each tag stores:

- entity type/name
- line number
- tag and value
- confidence
- source/evidence

This gives the graph a reasoning-oriented layer without hardcoding domain logic into every query.

Project-native tags now also describe Exorcist architecture directly, for example:

- `architecture:shell`
- `architecture:bootstrap`
- `architecture:shared-service`
- `architecture:shared-component`
- `architecture:base-plugin`
- `architecture:plugin-domain`
- `architecture:sdk-boundary`
- `ui:workbench-surface`
- `ui:menu-owner`
- `ui:toolbar-owner`
- `ui:dock-owner`
- `workspace:profile-activation`
- `workspace:template-system`

This makes the graph useful for architecture reasoning, not just code lookup.

## Indexing Pipeline

1. **scanFiles** — Walk workspace, insert file records, detect language
2. **parseAll** — Dispatch to `ILanguageIndexer::parseFile()` per language
3. **buildImplMap** — Match `.h` ↔ `.cpp` pairs
4. **buildSubsystemGraph** — Compute subsystem dependency edges
5. **buildFunctionIndex** — Extract functions with exact line ranges
6. **buildFileSummaries** — Generate one-line summaries from first comments
7. **buildEdges** — Create unified relationship edges
8. **scanQtConnections** — Find `connect()` calls (C++ only)
9. **scanServices** — Find `registerService`/`resolveService` calls
10. **scanTestCases** — Find test methods (QTest/Jest/pytest)
11. **scanCMakeTargets** — Parse CMakeLists.txt for targets
12. **buildCallGraph** — Build function-level call graph
13. **scanXmlBindings** — Scan XML/code bindings
14. **scanRuntimeEntrypoints** — Detect dynamic/runtime loading entry points
15. **buildSymbolAliases** — Detect likely aliases/typos
16. **buildReachability** — Compute reachable vs potentially dead functions
17. **buildHotspotScores** — Score risk/coupling/test gaps
18. **buildSemanticTags** — Derive semantic/security/performance/audit tags
19. **buildFtsIndex** — Populate FTS5 full-text search

## Query API

`CodeGraphQuery` provides JSON-returning methods for agent tools:

| Method | Purpose |
|--------|---------|
| `projectSummary()` | Overview: file/class/function counts, top subsystems |
| `functions(target)` | Find functions by file or class with line ranges |
| `context(target)` | Full context: summary, deps, rdeps, functions, tests, semantic tags |
| `semanticTags(target)` | All semantic tags for a file/class target |
| `searchByTag(tag)` | Find entities by semantic tag or tag value |

## Python Tools (`tools/`)

Companion Python scripts for offline analysis:

| Tool | Purpose |
|------|---------|
| `build_codegraph.py` | Full rebuild of `tools/codegraph.db` |
| `query_graph.py` | Interactive queries (context, search, functions, edges, tags, tagsearch) |
| `gap_report.py` | Feature gaps, header-only violations, orphans |
| `find_class.py` | Quick class/file/method lookup |
| `check_deps.py` | Impact analysis before editing a file |
| `gen_stub.py` | Generate `.cpp` skeleton from `.h` header |
| `style_check.py` | Coding convention checker (bare new/delete, header-only) |
| `test_coverage.py` | Test gap analysis by subsystem |
| `api_surface.py` | Extract public API of a class |
| `todo_extract.py` | Extract TODO/FIXME comments |
| `verify_graph.py` | Database integrity verification |
