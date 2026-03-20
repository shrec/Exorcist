#!/usr/bin/env python3
"""
Exorcist IDE — Code Graph Builder
Scans the entire project (src/, plugins/, server/, tests/, ReserchRepos/)
and builds a SQLite database mapping files, classes, methods, includes,
and implementation status.

Usage:
    python tools/build_codegraph.py
    # Creates tools/codegraph.db
"""

import hashlib
import os
import re
import sqlite3
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB_PATH = ROOT / "tools" / "codegraph.db"

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# Directories to scan
SCAN_DIRS = [
    "src",
    "plugins",
    "server",
    "tests",
    "ReserchRepos",
]

# File extensions to index
CODE_EXTS = {
    ".h", ".hpp", ".cpp", ".cc", ".cxx",
    ".ts", ".js", ".mjs",
    ".py",
    ".json",
    ".md",
}

SKIP_DIRS = {
    "node_modules", ".git", "build", "build-llvm", "build-ci",
    "build-release", "third_party", "_deps", "__pycache__",
    "CMakeFiles", ".vs", ".vscode",
}

# ── Regex patterns ──────────────────────────────────────────────────────────

# C++ class/struct declaration
RE_CLASS = re.compile(
    r'^\s*(?:class|struct)\s+'
    r'(?:Q_DECL_EXPORT\s+|Q_DECL_IMPORT\s+)?'
    r'(\w+)'
    r'(?:\s*:\s*(?:public|protected|private)\s+([\w:,\s]+?))?'
    r'\s*\{',
    re.MULTILINE
)

# C++ method declaration (in header)
RE_METHOD_DECL = re.compile(
    r'^\s*(?:virtual\s+|static\s+|explicit\s+|inline\s+|constexpr\s+)*'
    r'(?:[\w:<>,\s\*&]+?)\s+'     # return type
    r'(\w+)\s*\('                   # method name
    r'([^)]*)\)',                    # params
    re.MULTILINE
)

# C++ #include
RE_INCLUDE = re.compile(
    r'^\s*#include\s+[<"]([^>"]+)[>"]',
    re.MULTILINE
)

# Pure virtual (= 0)
RE_PURE_VIRTUAL = re.compile(r'=\s*0\s*;')

# C++ namespace
RE_NAMESPACE = re.compile(r'^\s*namespace\s+(\w+)', re.MULTILINE)

# Q_OBJECT macro
RE_QOBJECT = re.compile(r'Q_OBJECT')

# signals: / slots: sections
RE_SIGNALS = re.compile(r'^\s*signals\s*:', re.MULTILINE)
RE_SLOTS = re.compile(r'^\s*(?:public|private|protected)\s+slots\s*:', re.MULTILINE)

# TypeScript/JS class
RE_TS_CLASS = re.compile(
    r'^\s*(?:export\s+)?class\s+(\w+)'
    r'(?:\s+extends\s+(\w+))?'
    r'(?:\s+implements\s+([\w,\s]+?))?'
    r'\s*\{',
    re.MULTILINE
)

# TypeScript/JS function
RE_TS_FUNC = re.compile(
    r'^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\(',
    re.MULTILINE
)

# TypeScript import
RE_TS_IMPORT = re.compile(
    r"^\s*import\s+.+?\s+from\s+['\"]([^'\"]+)['\"]",
    re.MULTILINE
)

# ── Additional patterns for enhanced code graph ─────────────────────────

# Qt connect() — new-style: connect(sender, &Class::signal, receiver, &Class::slot)
RE_CONNECT = re.compile(
    r'connect\s*\(\s*'
    r'(\w+)\s*,\s*&(\w+)::\s*(\w+)\s*,'
    r'\s*(\w+)\s*,\s*&(\w+)::\s*(\w+)',
)

# ServiceRegistry register/resolve
RE_SERVICE_REG = re.compile(r'registerService\s*\(\s*"([^"]+)"')
RE_SERVICE_RES = re.compile(r'resolveService\s*\(\s*"([^"]+)"')

# QTest test methods (void testXxx())
RE_TEST_SLOT = re.compile(r'^\s*void\s+(test\w+)\s*\(\s*\)', re.MULTILINE)

# QTEST_MAIN(ClassName)
RE_QTEST_MAIN = re.compile(r'QTEST_MAIN\s*\(\s*(\w+)\s*\)')

# CMake targets
RE_CMAKE_ADD_EXEC = re.compile(r'add_executable\s*\(\s*(\S+)', re.MULTILINE)
RE_CMAKE_ADD_LIB = re.compile(r'add_library\s*\(\s*(\S+)', re.MULTILINE)
RE_CMAKE_ADD_TEST = re.compile(r'add_test\s*\(\s*(?:NAME\s+)?(\S+)', re.MULTILINE)

# Function call invocation — used for call graph (bare name followed by open-paren)
RE_FUNC_CALL = re.compile(r'\b([A-Za-z_]\w{2,})\s*\(')


def create_db(conn):
    """Create the schema."""
    conn.executescript("""
        DROP TABLE IF EXISTS files;
        DROP TABLE IF EXISTS classes;
        DROP TABLE IF EXISTS methods;
        DROP TABLE IF EXISTS includes;
        DROP TABLE IF EXISTS namespaces;
        DROP TABLE IF EXISTS features;
        DROP TABLE IF EXISTS implementations;
        DROP TABLE IF EXISTS subsystems;
        DROP TABLE IF EXISTS subsystem_deps;
        DROP TABLE IF EXISTS class_refs;
        DROP TABLE IF EXISTS file_summaries;
        DROP TABLE IF EXISTS function_index;
        DROP TABLE IF EXISTS edges;
        DROP TABLE IF EXISTS qt_connections;
        DROP TABLE IF EXISTS services;
        DROP TABLE IF EXISTS test_cases;
        DROP TABLE IF EXISTS cmake_targets;
        DROP TABLE IF EXISTS call_graph;
        DROP TABLE IF EXISTS xml_bindings;
        DROP TABLE IF EXISTS config_issues;
        DROP TABLE IF EXISTS runtime_entrypoints;
        DROP TABLE IF EXISTS symbol_aliases;
        DROP TABLE IF EXISTS reachability;
        DROP TABLE IF EXISTS hotspot_scores;
        DROP TABLE IF EXISTS semantic_tags;

        CREATE TABLE files (
            id          INTEGER PRIMARY KEY,
            path        TEXT UNIQUE NOT NULL,
            dir         TEXT NOT NULL,
            name        TEXT NOT NULL,
            ext         TEXT NOT NULL,
            lang        TEXT NOT NULL,
            lines       INTEGER DEFAULT 0,
            size_bytes  INTEGER DEFAULT 0,
            has_qobject INTEGER DEFAULT 0,
            has_signals INTEGER DEFAULT 0,
            has_slots   INTEGER DEFAULT 0,
            subsystem   TEXT DEFAULT ''
        );

        CREATE TABLE classes (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            name        TEXT NOT NULL,
            bases       TEXT,
            line_num    INTEGER,
            is_interface INTEGER DEFAULT 0,
            is_struct   INTEGER DEFAULT 0,
            has_impl    INTEGER DEFAULT 0,
            lang        TEXT DEFAULT 'cpp'
        );

        CREATE TABLE methods (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            class_id    INTEGER REFERENCES classes(id),
            name        TEXT NOT NULL,
            params      TEXT,
            line_num    INTEGER,
            is_pure_virtual INTEGER DEFAULT 0,
            is_signal   INTEGER DEFAULT 0,
            is_slot     INTEGER DEFAULT 0,
            has_body    INTEGER DEFAULT 0,
            lang        TEXT DEFAULT 'cpp'
        );

        CREATE TABLE includes (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            included    TEXT NOT NULL,
            is_system   INTEGER DEFAULT 0
        );

        CREATE TABLE namespaces (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            name        TEXT NOT NULL
        );

        CREATE TABLE implementations (
            id          INTEGER PRIMARY KEY,
            header_id   INTEGER REFERENCES files(id),
            source_id   INTEGER REFERENCES files(id),
            class_name  TEXT,
            method_count INTEGER DEFAULT 0
        );

        CREATE TABLE features (
            id          INTEGER PRIMARY KEY,
            name        TEXT UNIQUE NOT NULL,
            status      TEXT DEFAULT 'unknown',
            header_file TEXT,
            source_file TEXT,
            class_name  TEXT,
            notes       TEXT
        );

        -- Subsystem table: each directory under src/ is a subsystem
        CREATE TABLE subsystems (
            id          INTEGER PRIMARY KEY,
            name        TEXT UNIQUE NOT NULL,
            dir_path    TEXT NOT NULL,
            file_count  INTEGER DEFAULT 0,
            line_count  INTEGER DEFAULT 0,
            class_count INTEGER DEFAULT 0,
            h_only_count INTEGER DEFAULT 0,
            interface_count INTEGER DEFAULT 0
        );

        -- Subsystem dependency edges (from includes)
        CREATE TABLE subsystem_deps (
            id              INTEGER PRIMARY KEY,
            from_subsystem  TEXT NOT NULL,
            to_subsystem    TEXT NOT NULL,
            edge_count      INTEGER DEFAULT 1,
            UNIQUE(from_subsystem, to_subsystem)
        );

        -- Class cross-references (which class uses which)
        CREATE TABLE class_refs (
            id          INTEGER PRIMARY KEY,
            from_class  TEXT NOT NULL,
            to_class    TEXT NOT NULL,
            ref_type    TEXT DEFAULT 'include',
            UNIQUE(from_class, to_class, ref_type)
        );

        CREATE INDEX idx_files_path ON files(path);
        CREATE INDEX idx_files_dir ON files(dir);
        CREATE INDEX idx_files_subsystem ON files(subsystem);
        CREATE INDEX idx_classes_name ON classes(name);
        CREATE INDEX idx_classes_file ON classes(file_id);
        CREATE INDEX idx_methods_class ON methods(class_id);
        CREATE INDEX idx_methods_name ON methods(name);
        CREATE INDEX idx_includes_file ON includes(file_id);
        CREATE INDEX idx_includes_included ON includes(included);
        CREATE INDEX idx_subsystem_deps_from ON subsystem_deps(from_subsystem);
        CREATE INDEX idx_class_refs_from ON class_refs(from_class);

        -- File summaries: 1-line description per file
        CREATE TABLE file_summaries (
            file_id     INTEGER PRIMARY KEY REFERENCES files(id),
            summary     TEXT NOT NULL DEFAULT '',
            category    TEXT DEFAULT '',
            key_classes TEXT DEFAULT '',
            key_exports TEXT DEFAULT ''
        );

        -- Function/method index with exact line ranges
        CREATE TABLE function_index (
            id                INTEGER PRIMARY KEY,
            file_id           INTEGER REFERENCES files(id),
            name              TEXT NOT NULL,
            qualified_name    TEXT DEFAULT '',
            params            TEXT DEFAULT '',
            return_type       TEXT DEFAULT '',
            start_line        INTEGER NOT NULL,
            end_line          INTEGER NOT NULL,
            line_count        INTEGER DEFAULT 0,
            is_method         INTEGER DEFAULT 0,
            class_name        TEXT DEFAULT '',
            hot_path          INTEGER DEFAULT 0,
            ct_sensitive      INTEGER DEFAULT 0,
            gpu_candidate     INTEGER DEFAULT 0,
            batchable         INTEGER DEFAULT 0,
            recently_modified INTEGER DEFAULT 0,
            body_hash         TEXT    DEFAULT '',
            duplicate_group   TEXT    DEFAULT ''
        );

        -- Unified relationship edges
        CREATE TABLE edges (
            id          INTEGER PRIMARY KEY,
            source_file INTEGER REFERENCES files(id),
            target_file INTEGER,
            source_name TEXT DEFAULT '',
            target_name TEXT DEFAULT '',
            edge_type   TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0,
            metadata    TEXT DEFAULT ''
        );

        -- Qt signal/slot connections
        CREATE TABLE qt_connections (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            sender      TEXT NOT NULL,
            signal_name TEXT NOT NULL,
            receiver    TEXT NOT NULL,
            slot_name   TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0
        );

        -- ServiceRegistry entries
        CREATE TABLE services (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            service_key TEXT NOT NULL,
            class_name  TEXT DEFAULT '',
            line_num    INTEGER DEFAULT 0,
            reg_type    TEXT DEFAULT 'register'
        );

        -- QTest test cases
        CREATE TABLE test_cases (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            test_class  TEXT NOT NULL,
            test_method TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0
        );

        -- CMake build targets
        CREATE TABLE cmake_targets (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            target_name TEXT NOT NULL,
            target_type TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0
        );

        CREATE INDEX idx_function_index_file ON function_index(file_id);
        CREATE INDEX idx_function_index_name ON function_index(name);
        CREATE INDEX idx_function_index_class ON function_index(class_name);
        CREATE INDEX idx_edges_source ON edges(source_file);
        CREATE INDEX idx_edges_target ON edges(target_file);
        CREATE INDEX idx_edges_type ON edges(edge_type);
        CREATE INDEX idx_qt_connections_file ON qt_connections(file_id);
        CREATE INDEX idx_services_key ON services(service_key);
        CREATE INDEX idx_test_cases_file ON test_cases(file_id);
        CREATE INDEX idx_cmake_targets_file ON cmake_targets(file_id);
        CREATE INDEX idx_file_summaries_cat ON file_summaries(category);

        -- ── Call graph: function-to-function call edges ────────────────────
        CREATE TABLE call_graph (
            id          INTEGER PRIMARY KEY,
            caller_file INTEGER REFERENCES files(id),
            caller_func TEXT NOT NULL,
            callee_func TEXT NOT NULL,
            callee_file INTEGER,
            line_num    INTEGER DEFAULT 0
        );

        -- ── XML → C++ symbol bindings ──────────────────────────────────
        CREATE TABLE xml_bindings (
            id           INTEGER PRIMARY KEY,
            xml_file     INTEGER REFERENCES files(id),
            element      TEXT NOT NULL,
            attribute    TEXT DEFAULT '',
            value        TEXT DEFAULT '',
            bound_symbol TEXT DEFAULT '',
            bound_file   INTEGER,
            binding_type TEXT DEFAULT 'unresolved',
            line_num     INTEGER DEFAULT 0
        );

        -- ── Config / asset validation issues ───────────────────────────
        CREATE TABLE config_issues (
            id         INTEGER PRIMARY KEY,
            xml_file   INTEGER REFERENCES files(id),
            issue_type TEXT NOT NULL,
            element    TEXT DEFAULT '',
            attribute  TEXT DEFAULT '',
            value      TEXT DEFAULT '',
            message    TEXT NOT NULL,
            line_num   INTEGER DEFAULT 0,
            severity   TEXT DEFAULT 'warning'
        );

        -- ── Runtime loading entrypoints ────────────────────────────────
        CREATE TABLE runtime_entrypoints (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            loader_type TEXT NOT NULL,
            target      TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0
        );

        -- ── Symbol aliases: typo / rename-drift detection ─────────────────
        CREATE TABLE symbol_aliases (
            id            INTEGER PRIMARY KEY,
            symbol_a      TEXT NOT NULL,
            file_a        INTEGER,
            symbol_b      TEXT NOT NULL,
            file_b        INTEGER,
            edit_distance INTEGER DEFAULT 0,
            alias_type    TEXT DEFAULT 'similar'
        );

        -- ── Reachability: BFS from program entrypoints ───────────────────
        CREATE TABLE reachability (
            id            INTEGER PRIMARY KEY,
            file_id       INTEGER REFERENCES files(id),
            symbol        TEXT NOT NULL,
            symbol_type   TEXT DEFAULT 'function',
            is_reachable  INTEGER DEFAULT 1,
            reachable_via TEXT DEFAULT '',
            dead_reason   TEXT DEFAULT ''
        );

        -- ── Hotspot scores: coupling × coverage-gap × crash-risk ──────────────
        CREATE TABLE hotspot_scores (
            id               INTEGER PRIMARY KEY,
            file_id          INTEGER REFERENCES files(id) UNIQUE,
            coupling_in      INTEGER DEFAULT 0,
            coupling_out     INTEGER DEFAULT 0,
            has_tests        INTEGER DEFAULT 0,
            crash_indicators INTEGER DEFAULT 0,
            hotspot_score    REAL    DEFAULT 0.0,
            risk_factors     TEXT    DEFAULT ''
        );

        CREATE TABLE semantic_tags (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            entity_type TEXT NOT NULL,
            entity_name TEXT NOT NULL,
            line_num    INTEGER DEFAULT 0,
            tag         TEXT NOT NULL,
            tag_value   TEXT DEFAULT '',
            confidence  INTEGER DEFAULT 100,
            source      TEXT DEFAULT 'heuristic',
            evidence    TEXT DEFAULT '',
            UNIQUE(file_id, entity_type, entity_name, tag, tag_value)
        );

        CREATE INDEX idx_call_graph_caller   ON call_graph(caller_file);
        CREATE INDEX idx_call_graph_callee   ON call_graph(callee_func);
        CREATE INDEX idx_xml_bindings_file   ON xml_bindings(xml_file);
        CREATE INDEX idx_xml_bindings_type   ON xml_bindings(binding_type);
        CREATE INDEX idx_config_issues_file  ON config_issues(xml_file);
        CREATE INDEX idx_runtime_ep_file     ON runtime_entrypoints(file_id);
        CREATE INDEX idx_aliases_a           ON symbol_aliases(symbol_a);
        CREATE INDEX idx_aliases_dist        ON symbol_aliases(edit_distance);
        CREATE INDEX idx_reachability_file   ON reachability(file_id);
        CREATE INDEX idx_reachability_reach  ON reachability(is_reachable);
        CREATE INDEX idx_hotspot_score       ON hotspot_scores(hotspot_score);
        CREATE INDEX idx_semantic_tags_file  ON semantic_tags(file_id);
        CREATE INDEX idx_semantic_tags_tag   ON semantic_tags(tag);
        CREATE INDEX idx_semantic_tags_entity ON semantic_tags(entity_type, entity_name);

        -- ── Function-level AI metadata (summary, flags, hash) ─────────────
        CREATE TABLE function_summary (
            id                INTEGER PRIMARY KEY,
            file_id           INTEGER REFERENCES files(id),
            symbol            TEXT NOT NULL,
            qualified_symbol  TEXT DEFAULT '',
            summary           TEXT DEFAULT '',
            category          TEXT DEFAULT '',
            batchable         INTEGER DEFAULT 0,
            gpu_candidate     INTEGER DEFAULT 0,
            ct_sensitive      INTEGER DEFAULT 0,
            recently_modified INTEGER DEFAULT 0,
            body_hash         TEXT DEFAULT '',
            last_updated      TEXT DEFAULT '',
            UNIQUE(file_id, symbol)
        );

        -- ── Symbol slices: signature + critical lines (low-token context) ──
        CREATE TABLE symbol_slices (
            id                   INTEGER PRIMARY KEY,
            file_id              INTEGER REFERENCES files(id),
            symbol               TEXT NOT NULL,
            signature            TEXT DEFAULT '',
            critical_lines       TEXT DEFAULT '',
            slice_token_estimate INTEGER DEFAULT 0,
            full_token_estimate  INTEGER DEFAULT 0,
            UNIQUE(file_id, symbol)
        );

        -- ── Optimization knowledge patterns ────────────────────────────────
        CREATE TABLE optimization_patterns (
            id              INTEGER PRIMARY KEY,
            pattern_name    TEXT UNIQUE NOT NULL,
            description     TEXT DEFAULT '',
            gain            TEXT DEFAULT 'unknown',
            risk            TEXT DEFAULT 'low',
            applicable_when TEXT DEFAULT '',
            example_symbol  TEXT DEFAULT ''
        );

        -- ── Git delta: recently modified symbols ───────────────────────────
        CREATE TABLE git_delta (
            id            INTEGER PRIMARY KEY,
            file_id       INTEGER REFERENCES files(id),
            symbol        TEXT DEFAULT '',
            commit_hash   TEXT DEFAULT '',
            changed_at    TEXT DEFAULT '',
            diff_snippet  TEXT DEFAULT '',
            lines_added   INTEGER DEFAULT 0,
            lines_removed INTEGER DEFAULT 0
        );

        CREATE INDEX idx_funcidx_hot_path    ON function_index(hot_path);
        CREATE INDEX idx_funcidx_ct_sens     ON function_index(ct_sensitive);
        CREATE INDEX idx_funcidx_body_hash   ON function_index(body_hash);
        CREATE INDEX idx_func_summary_file   ON function_summary(file_id);
        CREATE INDEX idx_func_summary_sym    ON function_summary(symbol);
        CREATE INDEX idx_func_summary_flags  ON function_summary(gpu_candidate, batchable);
        CREATE INDEX idx_symbol_slices_file  ON symbol_slices(file_id);
        CREATE INDEX idx_symbol_slices_sym   ON symbol_slices(symbol);
        CREATE INDEX idx_git_delta_file      ON git_delta(file_id);

        -- ── Composite analysis scores (hotness × complexity × fan × gpu × ct) ──
        CREATE TABLE IF NOT EXISTS analysis_scores (
            id                 INTEGER PRIMARY KEY,
            file_id            INTEGER REFERENCES files(id),
            symbol_name        TEXT NOT NULL,
            file_path          TEXT NOT NULL,
            hotness_score      INTEGER DEFAULT 0,
            complexity_score   INTEGER DEFAULT 0,
            fanin_score        INTEGER DEFAULT 0,
            fanout_score       INTEGER DEFAULT 0,
            optimization_score INTEGER DEFAULT 0,
            gpu_score          INTEGER DEFAULT 0,
            ct_risk_score      INTEGER DEFAULT 0,
            audit_gap_score    INTEGER DEFAULT 0,
            overall_priority   INTEGER DEFAULT 0,
            reasons            TEXT DEFAULT '',
            UNIQUE(file_id, symbol_name)
        );

        -- ── AI task queue: auto-generated bottleneck tasks ────────────────────
        CREATE TABLE IF NOT EXISTS ai_tasks (
            id          INTEGER PRIMARY KEY,
            file_id     INTEGER REFERENCES files(id),
            task_type   TEXT NOT NULL,
            symbol_name TEXT NOT NULL,
            file_path   TEXT NOT NULL,
            prompt      TEXT NOT NULL,
            status      TEXT DEFAULT 'pending',
            priority    INTEGER DEFAULT 0,
            created_at  TEXT DEFAULT ''
        );

        CREATE INDEX IF NOT EXISTS idx_analysis_priority ON analysis_scores(overall_priority DESC);
        CREATE INDEX IF NOT EXISTS idx_analysis_file     ON analysis_scores(file_id);
        CREATE INDEX IF NOT EXISTS idx_analysis_gpu      ON analysis_scores(gpu_score DESC);
        CREATE INDEX IF NOT EXISTS idx_analysis_ct_risk  ON analysis_scores(ct_risk_score DESC);
        CREATE INDEX IF NOT EXISTS idx_ai_tasks_status   ON ai_tasks(status);
        CREATE INDEX IF NOT EXISTS idx_ai_tasks_priority ON ai_tasks(priority DESC);
    """)

    # Create v_bottleneck_queue view (references analysis_scores + file_summaries)
    try:
        conn.execute("DROP VIEW IF EXISTS v_bottleneck_queue")
        conn.execute("""
            CREATE VIEW v_bottleneck_queue AS
            SELECT
                a.symbol_name,
                a.file_path,
                a.hotness_score,
                a.complexity_score,
                a.fanin_score,
                a.fanout_score,
                a.gpu_score,
                a.ct_risk_score,
                a.audit_gap_score,
                a.overall_priority,
                a.reasons,
                COALESCE(fs.summary, '') AS summary
            FROM analysis_scores a
            LEFT JOIN files f ON a.file_id = f.id
            LEFT JOIN file_summaries fs ON fs.file_id = f.id
            ORDER BY a.overall_priority DESC, a.hotness_score DESC
        """)
    except Exception as e:
        print(f"  Warning: v_bottleneck_queue view: {e}")

    # FTS5 full-text search (optional — requires FTS5 extension)
    try:
        conn.execute("DROP TABLE IF EXISTS fts_index")
        conn.execute("""
            CREATE VIRTUAL TABLE fts_index USING fts5(
                name, qualified_name, file_path, kind, summary,
                tokenize='unicode61'
            )
        """)
    except Exception as e:
        print(f"  Warning: FTS5 not available ({e}), text search disabled")


def detect_lang(ext):
    if ext in (".h", ".hpp", ".cpp", ".cc", ".cxx"):
        return "cpp"
    if ext in (".ts",):
        return "typescript"
    if ext in (".js", ".mjs"):
        return "javascript"
    if ext == ".py":
        return "python"
    if ext == ".json":
        return "json"
    if ext == ".md":
        return "markdown"
    return "other"


def detect_subsystem(rel_path):
    """Derive subsystem name from relative file path."""
    parts = rel_path.split("/")
    if parts[0] == "src":
        if len(parts) >= 3:
            return parts[1]  # src/agent/*, src/editor/*, src/lsp/*, etc.
        return "core"  # src/*.h, src/*.cpp
    elif parts[0] == "plugins":
        if len(parts) >= 3:
            return f"plugin-{parts[1]}"  # plugins/copilot/* etc.
        return "plugins"
    elif parts[0] == "server":
        return "server"
    elif parts[0] == "tests":
        return "tests"
    elif parts[0] == "ReserchRepos":
        if len(parts) >= 2:
            return f"research-{parts[1]}"
        return "research"
    return "other"


def scan_files(conn):
    """Walk the project tree and index all source files."""
    count = 0
    for scan_dir in SCAN_DIRS:
        full_dir = ROOT / scan_dir
        if not full_dir.exists():
            print(f"  SKIP (not found): {scan_dir}/")
            continue

        for root_path, dirs, filenames in os.walk(full_dir):
            # Skip unwanted directories
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]

            for fname in filenames:
                fpath = Path(root_path) / fname
                ext = fpath.suffix.lower()
                if ext not in CODE_EXTS:
                    continue

                rel_path = str(fpath.relative_to(ROOT)).replace("\\", "/")
                dir_part = str(Path(root_path).relative_to(ROOT)).replace("\\", "/")
                lang = detect_lang(ext)

                try:
                    size = fpath.stat().st_size
                    if size > 2_000_000:  # skip files > 2MB
                        continue
                    content = fpath.read_text(encoding="utf-8", errors="replace")
                    line_count = content.count("\n") + 1
                except Exception:
                    continue

                has_qobj = 0
                has_sig = 0
                has_slt = 0
                if ext in (".h", ".hpp", ".cpp", ".cc"):
                    has_qobj = 1 if "Q_OBJECT" in content else 0
                    has_sig = 1 if "signals:" in content else 0
                    has_slt = 1 if "slots:" in content else 0

                conn.execute(
                    "INSERT INTO files (path, dir, name, ext, lang, lines, size_bytes, "
                    "has_qobject, has_signals, has_slots, subsystem) VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                    (rel_path, dir_part, fname, ext, lang, line_count, size,
                     has_qobj, has_sig, has_slt, detect_subsystem(rel_path))
                )
                count += 1

    conn.commit()
    print(f"  Indexed {count} files")
    return count


def parse_cpp_file(conn, file_id, path, content):
    """Extract classes, methods, includes from a C++ file."""
    # Includes
    for m in RE_INCLUDE.finditer(content):
        inc = m.group(1)
        is_sys = 1 if content[m.start():m.start()+20].strip().startswith("#include <") else 0
        conn.execute(
            "INSERT INTO includes (file_id, included, is_system) VALUES (?,?,?)",
            (file_id, inc, is_sys)
        )

    # Namespaces
    for m in RE_NAMESPACE.finditer(content):
        conn.execute(
            "INSERT INTO namespaces (file_id, name) VALUES (?,?)",
            (file_id, m.group(1))
        )

    # Classes
    lines = content.split("\n")
    for m in RE_CLASS.finditer(content):
        cls_name = m.group(1)
        bases = m.group(2).strip() if m.group(2) else ""
        line_num = content[:m.start()].count("\n") + 1

        # Check if interface (has pure virtual methods in the class body)
        # Find the class body
        brace_start = m.end() - 1
        depth = 1
        pos = brace_start + 1
        class_body = ""
        while pos < len(content) and depth > 0:
            if content[pos] == "{":
                depth += 1
            elif content[pos] == "}":
                depth -= 1
            pos += 1
        class_body = content[brace_start:pos]

        is_iface = 1 if RE_PURE_VIRTUAL.search(class_body) else 0
        is_struct = 1 if content[m.start():m.start()+20].strip().startswith("struct") else 0

        conn.execute(
            "INSERT INTO classes (file_id, name, bases, line_num, is_interface, is_struct, lang) "
            "VALUES (?,?,?,?,?,?,?)",
            (file_id, cls_name, bases, line_num, is_iface, is_struct, "cpp")
        )


def parse_ts_file(conn, file_id, path, content):
    """Extract classes, functions, imports from TypeScript/JavaScript."""
    # Imports
    for m in RE_TS_IMPORT.finditer(content):
        conn.execute(
            "INSERT INTO includes (file_id, included, is_system) VALUES (?,?,?)",
            (file_id, m.group(1), 0)
        )

    # Classes
    for m in RE_TS_CLASS.finditer(content):
        cls_name = m.group(1)
        bases = m.group(2) or ""
        line_num = content[:m.start()].count("\n") + 1
        conn.execute(
            "INSERT INTO classes (file_id, name, bases, line_num, is_interface, is_struct, lang) "
            "VALUES (?,?,?,?,?,?,?)",
            (file_id, cls_name, bases, line_num, 0, 0, "typescript")
        )

    # Functions
    for m in RE_TS_FUNC.finditer(content):
        func_name = m.group(1)
        line_num = content[:m.start()].count("\n") + 1
        conn.execute(
            "INSERT INTO methods (file_id, class_id, name, params, line_num, lang) "
            "VALUES (?,?,?,?,?,?)",
            (file_id, None, func_name, "", line_num, "typescript")
        )


def parse_all(conn):
    """Parse all indexed files for structure."""
    rows = conn.execute("SELECT id, path, lang FROM files").fetchall()
    cpp_count = 0
    ts_count = 0

    for file_id, path, lang in rows:
        full_path = ROOT / path
        try:
            content = full_path.read_text(encoding="utf-8", errors="replace")
        except (Exception, BaseException):
            continue

        if lang == "cpp":
            parse_cpp_file(conn, file_id, path, content)
            cpp_count += 1
        elif lang in ("typescript", "javascript"):
            parse_ts_file(conn, file_id, path, content)
            ts_count += 1

    conn.commit()
    print(f"  Parsed {cpp_count} C++ files, {ts_count} TS/JS files")


def build_impl_map(conn):
    """Map header files to their source implementations."""
    headers = conn.execute(
        "SELECT id, path, name FROM files WHERE ext = '.h'"
    ).fetchall()

    impl_count = 0
    for hdr_id, hdr_path, hdr_name in headers:
        base = hdr_name.rsplit(".", 1)[0]
        hdr_dir = str(Path(hdr_path).parent)

        # Look for .cpp with same base name in same or nearby directory
        sources = conn.execute(
            "SELECT id, path FROM files WHERE name LIKE ? AND ext = '.cpp'",
            (base + ".cpp",)
        ).fetchall()

        for src_id, src_path in sources:
            # Get class names from header
            classes = conn.execute(
                "SELECT name FROM classes WHERE file_id = ?", (hdr_id,)
            ).fetchall()

            for (cls_name,) in classes:
                # Check if source file contains ClassName:: pattern
                try:
                    src_content = (ROOT / src_path).read_text(
                        encoding="utf-8", errors="replace"
                    )
                    pattern = f"{cls_name}::"
                    method_count = src_content.count(pattern)
                    if method_count > 0:
                        conn.execute(
                            "INSERT INTO implementations "
                            "(header_id, source_id, class_name, method_count) "
                            "VALUES (?,?,?,?)",
                            (hdr_id, src_id, cls_name, method_count)
                        )
                        # Mark class as having implementation
                        conn.execute(
                            "UPDATE classes SET has_impl = 1 "
                            "WHERE file_id = ? AND name = ?",
                            (hdr_id, cls_name)
                        )
                        impl_count += 1
                except Exception:
                    pass

    conn.commit()
    print(f"  Mapped {impl_count} class implementations")


def build_subsystem_graph(conn):
    """Build subsystem nodes and inter-subsystem dependency edges."""
    # Build subsystem stats
    subsys_rows = conn.execute("""
        SELECT subsystem, COUNT(*), SUM(lines)
        FROM files
        WHERE lang = 'cpp' AND path LIKE 'src/%'
        GROUP BY subsystem
        ORDER BY SUM(lines) DESC
    """).fetchall()

    for name, fcount, lcount in subsys_rows:
        cls_count = conn.execute(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.lang = 'cpp'", (name,)
        ).fetchone()[0]
        ho_count = conn.execute(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.has_impl = 0 AND c.is_interface = 0 "
            "AND c.lang = 'cpp' AND f.ext = '.h'", (name,)
        ).fetchone()[0]
        iface_count = conn.execute(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.is_interface = 1", (name,)
        ).fetchone()[0]
        dir_path = f"src/{name}" if name != "core" else "src"
        conn.execute(
            "INSERT OR REPLACE INTO subsystems "
            "(name, dir_path, file_count, line_count, class_count, h_only_count, interface_count) "
            "VALUES (?,?,?,?,?,?,?)",
            (name, dir_path, fcount, lcount or 0, cls_count, ho_count, iface_count)
        )

    # Build dependency edges: if file in subsystem A includes a header
    # from subsystem B, that's a dependency edge A -> B
    edges = conn.execute("""
        SELECT f1.subsystem AS from_sub, f2.subsystem AS to_sub, COUNT(*) AS cnt
        FROM includes i
        JOIN files f1 ON i.file_id = f1.id
        JOIN files f2 ON f2.path LIKE '%/' || i.included
                      OR f2.path = i.included
                      OR f2.name = i.included
        WHERE f1.subsystem != ''
          AND f2.subsystem != ''
          AND f1.subsystem != f2.subsystem
          AND f1.path LIKE 'src/%'
          AND f2.path LIKE 'src/%'
        GROUP BY f1.subsystem, f2.subsystem
    """).fetchall()

    for from_sub, to_sub, cnt in edges:
        conn.execute(
            "INSERT OR REPLACE INTO subsystem_deps "
            "(from_subsystem, to_subsystem, edge_count) VALUES (?,?,?)",
            (from_sub, to_sub, cnt)
        )

    # Build class cross-references via inheritance
    classes_with_bases = conn.execute(
        "SELECT c.name, c.bases FROM classes c WHERE c.bases IS NOT NULL AND c.bases != ''"
    ).fetchall()
    for cls_name, bases_str in classes_with_bases:
        for base in bases_str.split(","):
            base = base.strip().split("::")[-1].strip()
            if base and base != cls_name:
                try:
                    conn.execute(
                        "INSERT OR IGNORE INTO class_refs "
                        "(from_class, to_class, ref_type) VALUES (?,?,?)",
                        (cls_name, base, "inherits")
                    )
                except Exception:
                    pass

    conn.commit()
    sub_count = conn.execute("SELECT COUNT(*) FROM subsystems").fetchone()[0]
    dep_count = conn.execute("SELECT COUNT(*) FROM subsystem_deps").fetchone()[0]
    ref_count = conn.execute("SELECT COUNT(*) FROM class_refs").fetchone()[0]
    print(f"  {sub_count} subsystems, {dep_count} dependency edges, {ref_count} class refs")


def detect_features(conn):
    """Auto-detect feature status from code structure."""
    features = [
        # (name, header_pattern, source_pattern, class_name)
        ("InlineChat", "%inlinechat%", "%inlinechat%", "InlineChatWidget"),
        ("GhostText", "%inlinecompletion%", "%inlinecompletion%", "InlineCompletionEngine"),
        ("NextEditSuggestions", "%nextedit%", "%nextedit%", "NextEditPredictor"),
        ("QuickChat", "%quickchat%", "%quickchat%", "QuickChatDialog"),
        ("MergeConflict", "%mergeconflict%", "%mergeconflict%", "MergeConflictResolver"),
        ("SecureKeyStorage", "%securekeystorage%", "%securekeystorage%", "SecureKeyStorage"),
        ("SessionHistory", "%sessionhistory%", "%session%", "SessionStore"),
        ("WorkspaceEdit", "%workspaceedit%", "%workspaceedit%", "ChatWorkspaceEditWidget"),
        ("TestGeneration", "%testgen%", "%testgen%", "TestGenerator"),
        ("NetworkMonitor", "%networkmonitor%", "%networkmonitor%", "NetworkMonitor"),
        ("Telemetry", "%telemetry%", "%telemetry%", "TelemetryManager"),
        ("Notebook", "%notebook%", "%notebook%", "NotebookManager"),
        ("Trajectory", "%trajectory%", "%trajectory%", "TrajectoryRecorder"),
        ("CodeReview", "%reviewmanager%", "%reviewmanager%", "ReviewManager"),
        ("ContextPruning", "%contextbuilder%", "%contextbuilder%", "ContextBuilder"),
        ("SemanticSearch", "%semanticsearch%", "%semanticsearch%", "SemanticSearchTool"),
        ("RateLimit", "%errorstate%", "%errorstate%", "ErrorStateWidget"),
        ("CustomInstructions", "%promptfileloader%", "%promptfileloader%", "PromptFileLoader"),
        ("ChatPanel", "%chatpanel%", "%chatpanel%", "AgentChatPanel"),
        ("ChatInput", "%chatinput%", "%chatinput%", "ChatInputWidget"),
        ("ChatTurn", "%chatturn%", "%chatturn%", "ChatTurnWidget"),
        ("ChatMarkdown", "%chatmarkdown%", "%chatmarkdown%", "ChatMarkdownWidget"),
        ("ChatThinking", "%chatthinking%", "%chatthinking%", "ChatThinkingWidget"),
        ("ChatFollowups", "%chatfollowups%", "%chatfollowups%", "ChatFollowupsWidget"),
        ("ChatWelcome", "%chatwelcome%", "%chatwelcome%", "ChatWelcomeWidget"),
        ("ToolInvocation", "%toolinvocation%", "%toolinvocation%", "ChatToolInvocationWidget"),
        ("AgentController", "%agentcontroller%", "%agentcontroller%", "AgentController"),
        ("AgentOrchestrator", "%agentorchestrator%", "%agentorchestrator%", "AgentOrchestrator"),
        ("PluginManager", "%pluginmanager%", "%pluginmanager%", "PluginManager"),
        ("ServiceRegistry", "%serviceregistry%", "%serviceregistry%", "ServiceRegistry"),
        ("LSPClient", "%lspclient%", "%lspclient%", "LspClient"),
        ("Terminal", "%terminalwidget%", "%terminalwidget%", "TerminalWidget"),
        ("GitIntegration", "%gitservice%", "%gitservice%", "GitService"),
        ("MCPClient", "%mcpclient%", "%mcpclient%", "McpClient"),
        ("DebugAdapter", "%gdbmiadapter%", "%gdbmiadapter%", "GdbMiAdapter"),
        ("RemoteSSH", "%remotessh%", "%remotessh%", "RemoteSessionManager"),
        ("SearchFramework", "%searchworker%", "%searchworker%", "SearchWorker"),
        ("ProjectBrain", "%projectbrain%", "%projectbrain%", "ProjectBrainService"),
        ("ThemeManager", "%thememanager%", "%thememanager%", "ThemeManager"),
        ("CommandPalette", "%commandpalette%", "%commandpalette%", "CommandPalette"),
        ("CopilotProvider", "%copilotprovider%", "%copilotprovider%", "CopilotProvider"),
        ("ClaudeProvider", "%claudeprovider%", "%claudeprovider%", "ClaudeProvider"),
        ("OllamaProvider", "%ollamaprovider%", "%ollamaprovider%", "OllamaProvider"),
        ("BYOKProvider", "%byokprovider%", "%byokprovider%", "BYOKProvider"),
        # Additional subsystems
        ("Editor", "%editorview%", "%editorview%", "EditorView"),
        ("DockFramework", "%dockmanager%", "%dockmanager%", "DockManager"),
        ("BuildSystem", "%buildbootstrap%", "%buildbootstrap%", "BuildBootstrap"),
        ("InlineReview", "%inlinereview%", "%inlinereview%", "InlineReviewWidget"),
        ("GitPanel", "%gitpanel%", "%gitpanel%", "GitPanel"),
        ("DebugPanel", "%debugpanel%", "%debugpanel%", "DebugPanel"),
        ("McpPanel", "%mcppanel%", "%mcppanel%", "McpPanel"),
        ("SymbolOutline", "%symboloutline%", "%symboloutline%", "SymbolOutlinePanel"),
        ("BreadcrumbBar", "%breadcrumb%", "%breadcrumb%", "BreadcrumbBar"),
        ("Minimap", "%minimap%", "%minimap%", "MinimapWidget"),
        ("DiffViewer", "%diffviewer%", "%diffviewer%", "DiffViewerPanel"),
        ("ProposedEditPanel", "%proposededit%", "%proposededit%", "ProposedEditPanel"),
    ]

    for name, hdr_pat, src_pat, cls_name in features:
        # Find header
        hdr = conn.execute(
            "SELECT path FROM files WHERE lower(path) LIKE ? AND ext = '.h' LIMIT 1",
            (hdr_pat,)
        ).fetchone()
        hdr_path = hdr[0] if hdr else None

        # Find source
        src = conn.execute(
            "SELECT path FROM files WHERE lower(path) LIKE ? AND ext = '.cpp' LIMIT 1",
            (src_pat,)
        ).fetchone()
        src_path = src[0] if src else None

        # Check if class has implementation
        has_impl = conn.execute(
            "SELECT has_impl FROM classes WHERE name = ? LIMIT 1",
            (cls_name,)
        ).fetchone()

        if not hdr_path and not src_path:
            status = "missing"
        elif hdr_path and not src_path:
            status = "header-only"
        elif hdr_path and src_path:
            if has_impl and has_impl[0]:
                status = "implemented"
            else:
                status = "stub"
        else:
            status = "source-only"

        conn.execute(
            "INSERT OR REPLACE INTO features (name, status, header_file, source_file, class_name) "
            "VALUES (?,?,?,?,?)",
            (name, status, hdr_path, src_path, cls_name)
        )

    conn.commit()


# ══════════════════════════════════════════════════════════════════════════
# Enhanced code graph builders
# ══════════════════════════════════════════════════════════════════════════

def _find_brace_body(lines, start_idx, n):
    """From start_idx, look ahead for '{' and find its matching '}'.
    Returns (start_idx, end_line_idx) or None.
    Gives up if ';' appears before '{'."""
    for j in range(start_idx, min(start_idx + 15, n)):
        if '{' in lines[j]:
            depth = 0
            for k in range(j, n):
                depth += lines[k].count('{') - lines[k].count('}')
                if depth <= 0:
                    return (start_idx, k)
            return None
        if ';' in lines[j] and '{' not in lines[j]:
            return None
    return None


_SKIP_KEYWORDS = frozenset({
    'if', 'else', 'for', 'while', 'switch', 'do', 'return',
    'catch', 'try', 'case', 'class', 'struct', 'enum',
    'namespace', 'typedef', 'using', 'template', 'throw', 'delete',
})


def _insert_semantic_tag(conn, file_id, entity_type, entity_name, line_num,
                         tag, tag_value="", confidence=100,
                         source="heuristic", evidence=""):
    conn.execute(
        "INSERT OR IGNORE INTO semantic_tags "
        "(file_id, entity_type, entity_name, line_num, tag, tag_value, confidence, source, evidence) "
        "VALUES (?,?,?,?,?,?,?,?,?)",
        (file_id, entity_type, entity_name, line_num, tag, tag_value, confidence, source, evidence)
    )


def _infer_semantic_tags(path, lang, subsystem, content, entity_name=""):
    lower_path = path.lower()
    lower_content = content.lower()
    lower_name = entity_name.lower()
    tags = [f"lang:{lang}", "layer:semantic"]
    if subsystem:
        tags.append(f"subsystem:{subsystem}")

    def add_if(cond, value):
        if cond:
            tags.append(value)

    add_if("/plugin" in lower_path or subsystem.startswith("plugin-"), "semantic:plugin")
    add_if("/agent/" in lower_path or "agent" in lower_name, "semantic:agent")
    add_if("/build/" in lower_path or "build" in lower_name, "semantic:build")
    add_if("/debug/" in lower_path or "debug" in lower_name, "semantic:debug")
    add_if("/test" in lower_path or lower_name.startswith("test"), "semantic:test")
    add_if("/search/" in lower_path or "search" in lower_name, "semantic:search")
    add_if("/git/" in lower_path or "git" in lower_name, "semantic:git")
    add_if("/terminal/" in lower_path or "terminal" in lower_name, "semantic:terminal")
    add_if("/project/" in lower_path or "project" in lower_name, "semantic:project")
    add_if("/profile/" in lower_path or "profile" in lower_name, "semantic:profile")
    add_if("/dock/" in lower_path or "dock" in lower_name, "semantic:docking")
    add_if("/ui/" in lower_path or "widget" in lower_name or "panel" in lower_name or "dialog" in lower_name, "semantic:ui")
    add_if("/codegraph/" in lower_path or "index" in lower_name or "graph" in lower_name or "query" in lower_name, "semantic:analysis")
    add_if("registerservice" in lower_content or "serviceregistry" in lower_content, "semantic:service-registry")
    add_if("q_object" in lower_content or "signals:" in lower_content or "slots:" in lower_content, "semantic:qt")
    add_if("qprocess" in lower_content or "start(" in lower_content, "semantic:process")
    add_if("command" in lower_content or "command" in lower_name, "semantic:command")
    add_if("network" in lower_content or "qnetwork" in lower_content, "security:network-surface")
    add_if("auth" in lower_content or "token" in lower_content, "security:auth-surface")
    add_if("permission" in lower_content or "secure" in lower_content or "security" in lower_content, "security:sensitive")
    add_if("async" in lower_content or "qtconcurrent" in lower_content or "qtimer::singleshot" in lower_content, "performance:async")

    # Exorcist-native architecture tags
    add_if(lower_path in ("src/mainwindow.cpp", "src/mainwindow.h"), "architecture:shell")
    add_if("/bootstrap/" in lower_path, "architecture:bootstrap")
    add_if("/sdk/" in lower_path or "ihostservices" in lower_name or lower_name.startswith("i"), "architecture:sdk-boundary")
    add_if("/core/" in lower_path, "architecture:core-interface")
    add_if("/component/" in lower_path or "componentregistry" in lower_content or "component" in lower_name, "architecture:shared-component")
    add_if("/settings/" in lower_path or "/profile/" in lower_path or "sharedservicesbootstrap" in lower_path, "architecture:shared-service")
    add_if("/plugin/" in lower_path and "workbenchpluginbase" in lower_name, "architecture:base-plugin")
    add_if(subsystem.startswith("plugin-") or lower_path.startswith("plugins/"), "architecture:plugin-domain")
    add_if("workbenchpluginbase" in lower_content or "plugin" in lower_name, "architecture:plugin-owned")
    add_if("idockmanager" in lower_content or "imenumanager" in lower_content or "itoolbarmanager" in lower_content or "istatusbarmanager" in lower_content, "ui:workbench-surface")
    add_if("addmenucommand" in lower_content or "createmenu" in lower_content or "imenumanager" in lower_content, "ui:menu-owner")
    add_if("createtoolbar" in lower_content or "itoolbarmanager" in lower_content, "ui:toolbar-owner")
    add_if("dock(" in lower_content or "showdock" in lower_content or "idockmanager" in lower_content, "ui:dock-owner")
    add_if("iprofilemanager" in lower_content or "loadprofilesfromdirectory" in lower_content or "profilemanager" in lower_name, "workspace:profile-activation")
    add_if("projecttemplate" in lower_name or "projecttemplateregistry" in lower_content, "workspace:template-system")
    return sorted(set(tags))


def extract_cpp_functions(content):
    """Extract C++ function/method definitions with line ranges."""
    lines = content.split('\n')
    functions = []
    n = len(lines)
    i = 0
    in_block_comment = False

    while i < n:
        line = lines[i]
        stripped = line.strip()

        if in_block_comment:
            if '*/' in stripped:
                in_block_comment = False
            i += 1
            continue
        if stripped.startswith('/*') and '*/' not in stripped:
            in_block_comment = True
            i += 1
            continue

        if not stripped or stripped.startswith('#') or stripped.startswith('//'):
            i += 1
            continue

        # Method definition: ClassName::methodName(
        m = re.search(r'(\w+)::(~?\w+)\s*\(', stripped)
        if m:
            class_name = m.group(1)
            method_name = m.group(2)
            if method_name not in _SKIP_KEYWORDS:
                body = _find_brace_body(lines, i, n)
                if body:
                    _, end_i = body
                    functions.append({
                        'name': method_name,
                        'qualified_name': f'{class_name}::{method_name}',
                        'class_name': class_name,
                        'is_method': 1,
                        'start_line': i + 1,
                        'end_line': end_i + 1,
                    })
                    i = end_i + 1
                    continue

        # Free function at column 0
        if line and not line[0].isspace() and '(' in stripped and '::' not in stripped:
            fm = re.match(
                r'(?:static\s+|inline\s+|constexpr\s+|extern\s+)*'
                r'(?:[\w:*&<>, ]+\s+)'
                r'(\w+)\s*\(',
                stripped
            )
            if fm and fm.group(1) not in _SKIP_KEYWORDS:
                body = _find_brace_body(lines, i, n)
                if body:
                    _, end_i = body
                    functions.append({
                        'name': fm.group(1),
                        'qualified_name': fm.group(1),
                        'class_name': '',
                        'is_method': 0,
                        'start_line': i + 1,
                        'end_line': end_i + 1,
                    })
                    i = end_i + 1
                    continue

        i += 1

    return functions


def extract_python_functions(content):
    """Extract Python function/method definitions with line ranges."""
    lines = content.split('\n')
    functions = []
    n = len(lines)
    current_class = ''

    for i, line in enumerate(lines):
        cm = re.match(r'^class\s+(\w+)', line)
        if cm:
            current_class = cm.group(1)
            continue

        m = re.match(r'^(\s*)def\s+(\w+)\s*\(', line)
        if m:
            indent = len(m.group(1))
            func_name = m.group(2)
            end = n
            for j in range(i + 1, n):
                if lines[j].strip():
                    line_indent = len(lines[j]) - len(lines[j].lstrip())
                    if line_indent <= indent:
                        end = j
                        break

            is_method = indent > 0 and current_class != ''
            cls = current_class if is_method else ''
            qname = f'{cls}.{func_name}' if cls else func_name
            functions.append({
                'name': func_name,
                'qualified_name': qname,
                'class_name': cls,
                'is_method': 1 if is_method else 0,
                'start_line': i + 1,
                'end_line': end,
            })

    return functions


def build_function_index(conn):
    """Build function index with exact line ranges for all code files."""
    rows = conn.execute(
        "SELECT id, path, lang FROM files WHERE lang IN ('cpp', 'python') AND ext != '.h'"
    ).fetchall()
    count = 0

    for file_id, path, lang in rows:
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        if lang == 'cpp':
            funcs = extract_cpp_functions(content)
        elif lang == 'python':
            funcs = extract_python_functions(content)
        else:
            continue

        src_lines = content.split('\n')
        for f in funcs:
            body_lines = src_lines[max(0, f['start_line'] - 1):f['end_line']]
            body_text = '\n'.join(body_lines)
            body_hash = hashlib.sha256(
                body_text.encode('utf-8', errors='replace')
            ).hexdigest()[:16]

            lower_body = body_text.lower()
            lower_name = f['name'].lower()
            hot_path = 1 if any(kw in lower_name for kw in (
                'multiply', 'hash', 'compute', 'encode', 'decode',
                'compress', 'encrypt', 'decrypt', 'sign', 'verify',
                'inverse', 'reduce', 'square', 'double', 'add',
            )) or any(kw in lower_body for kw in (
                'loop', '__builtin', 'simd', 'vectorize', 'unroll',
            )) else 0
            ct_sensitive = 1 if any(kw in lower_body or kw in lower_name for kw in (
                'auth', 'token', 'password', 'secret', 'key',
                'nonce', 'hmac', 'aes', 'rsa', 'ecdsa', 'sha256',
                'cert', 'privkey', 'signing', 'ctcheck',
            )) else 0
            gpu_candidate = 1 if any(kw in lower_body or kw in lower_name for kw in (
                'parallel', 'vectorize', 'opencl', 'cuda',
                'batch_mul', 'batch_inv', 'batch_',
                'fft', 'ntt', 'convolution', 'matmul',
            )) else 0
            batchable = 1 if (
                (f['end_line'] - f['start_line'] + 1) > 4
                and any(kw in lower_body for kw in ('for (', 'for(', 'while (', 'while(',
                                                     'foreach', 'std::for_each', '.begin()'))
            ) else 0

            conn.execute(
                "INSERT INTO function_index "
                "(file_id, name, qualified_name, params, return_type, "
                "start_line, end_line, line_count, is_method, class_name, "
                "hot_path, ct_sensitive, gpu_candidate, batchable, body_hash) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (file_id, f['name'], f.get('qualified_name', f['name']),
                 f.get('params', ''), f.get('return_type', ''),
                 f['start_line'], f['end_line'],
                 f['end_line'] - f['start_line'] + 1,
                 f.get('is_method', 0), f.get('class_name', ''),
                 hot_path, ct_sensitive, gpu_candidate, batchable, body_hash)
            )
            count += 1

    conn.commit()
    print(f"  Indexed {count} functions/methods with line ranges")


def generate_file_summary(conn, file_id, path, content, lang):
    """Generate a 1-line summary, category, and key classes for a file."""
    lines = content.split('\n')
    first_comment = ''

    if lang == 'cpp':
        for li, ln in enumerate(lines[:30]):
            s = ln.strip()
            if s.startswith('///') or s.startswith('//!'):
                first_comment = s.lstrip('/!').strip()
                break
            if s.startswith('/**'):
                text = s[3:].strip().rstrip('*/')
                if text:
                    first_comment = text
                    break
                for nxt in lines[li + 1:li + 5]:
                    text = nxt.strip().lstrip('*').strip()
                    if text and text != '/':
                        first_comment = text
                        break
                break
            if s.startswith('#pragma once') or s.startswith('#include'):
                continue
            if s and not s.startswith('/*') and not s.startswith('//'):
                break
    elif lang == 'python':
        for ln in lines[:15]:
            s = ln.strip()
            if s.startswith('#') and not s.startswith('#!'):
                first_comment = s.lstrip('#').strip()
                break
            if '"""' in s or "'''" in s:
                first_comment = s.strip("\"' \t")
                break

    classes = conn.execute(
        "SELECT name, is_interface FROM classes WHERE file_id = ?", (file_id,)
    ).fetchall()
    class_names = [c[0] for c in classes]

    ext = Path(path).suffix.lower()
    base = Path(path).stem.lower()
    category = 'source'

    if 'test' in path.lower() or base.startswith('test_'):
        category = 'test'
    elif ext in ('.h', '.hpp'):
        if any(c[1] for c in classes):
            category = 'interface'
        elif any(kw in cn for cn in class_names
                 for kw in ('Widget', 'Panel', 'View', 'Dialog')):
            category = 'widget'
        elif any(kw in cn for cn in class_names
                 for kw in ('Manager', 'Service', 'Registry', 'Engine')):
            category = 'service'
        else:
            category = 'header'
    elif ext in ('.cpp', '.cc'):
        if any(kw in cn for cn in class_names
               for kw in ('Widget', 'Panel', 'View', 'Dialog')):
            category = 'widget'
        elif any(kw in cn for cn in class_names
                 for kw in ('Manager', 'Service', 'Registry', 'Engine')):
            category = 'service'
        else:
            category = 'implementation'
    elif ext == '.py':
        category = 'tool'
    elif ext == '.json':
        category = 'config'
    elif ext == '.md':
        category = 'doc'
    elif ext == '.cmake' or base == 'cmakelists':
        category = 'build'

    if first_comment and len(first_comment) > 5:
        summary = first_comment[:200]
    elif class_names:
        cat_label = {
            'interface': 'Interface', 'widget': 'UI widget',
            'service': 'Service', 'test': 'Tests for',
            'header': 'Header', 'implementation': 'Implementation of',
        }.get(category, 'Defines')
        summary = f"{cat_label}: {', '.join(class_names[:5])}"
    elif ext == '.md':
        for ln in lines[:5]:
            s = ln.strip().lstrip('#').strip()
            if s:
                summary = s[:200]
                break
        else:
            summary = Path(path).name
    else:
        summary = Path(path).name

    return summary, category, ', '.join(class_names[:10])


def build_file_summaries(conn):
    """Build 1-line summaries for all indexed files."""
    rows = conn.execute("SELECT id, path, lang FROM files").fetchall()
    count = 0

    for file_id, path, lang in rows:
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        summary, category, key_classes = generate_file_summary(
            conn, file_id, path, content, lang
        )
        conn.execute(
            "INSERT INTO file_summaries (file_id, summary, category, key_classes) "
            "VALUES (?,?,?,?)",
            (file_id, summary, category, key_classes)
        )
        count += 1

    conn.commit()
    cats = conn.execute(
        "SELECT category, COUNT(*) FROM file_summaries GROUP BY category ORDER BY COUNT(*) DESC"
    ).fetchall()
    cat_str = ', '.join(f"{c}: {n}" for c, n in cats)
    print(f"  Summarized {count} files ({cat_str})")


def build_edges(conn):
    """Build unified edge table from all relationship sources."""
    count = 0

    # Build filename -> [file_ids] mapping for include resolution
    name_to_ids = {}
    for row in conn.execute("SELECT id, name FROM files"):
        name_to_ids.setdefault(row[1], []).append(row[0])

    # 1. Include edges (file -> file, deduplicated)
    seen = set()
    for src_fid, included in conn.execute(
        "SELECT file_id, included FROM includes WHERE is_system = 0"
    ):
        inc_name = included.split('/')[-1]
        targets = name_to_ids.get(inc_name, [])
        for tgt_fid in targets:
            if tgt_fid != src_fid and (src_fid, tgt_fid) not in seen:
                seen.add((src_fid, tgt_fid))
                conn.execute(
                    "INSERT INTO edges (source_file, target_file, edge_type) "
                    "VALUES (?,?,?)",
                    (src_fid, tgt_fid, 'includes')
                )
                count += 1

    # 2. Implementation edges (source implements header class)
    for hdr_id, src_id, cls_name in conn.execute(
        "SELECT header_id, source_id, class_name FROM implementations"
    ):
        conn.execute(
            "INSERT INTO edges (source_file, target_file, source_name, "
            "target_name, edge_type, metadata) VALUES (?,?,?,?,?,?)",
            (src_id, hdr_id, cls_name, cls_name, 'implements', cls_name)
        )
        count += 1

    # 3. Inheritance edges (class -> base class)
    for row in conn.execute(
        "SELECT cr.from_class, cr.to_class, f1.id, f2.id "
        "FROM class_refs cr "
        "JOIN classes c1 ON c1.name = cr.from_class "
        "JOIN files f1 ON c1.file_id = f1.id "
        "JOIN classes c2 ON c2.name = cr.to_class "
        "JOIN files f2 ON c2.file_id = f2.id "
        "WHERE cr.ref_type = 'inherits'"
    ):
        conn.execute(
            "INSERT INTO edges (source_file, target_file, source_name, "
            "target_name, edge_type) VALUES (?,?,?,?,?)",
            (row[2], row[3], row[0], row[1], 'inherits')
        )
        count += 1

    # 4. Test edges (test file -> tested file)
    for row in conn.execute(
        "SELECT id, path, name FROM files WHERE path LIKE 'tests/%' AND ext = '.cpp'"
    ):
        test_fid, test_path, test_name = row
        base = test_name.replace('test_', '').replace('.cpp', '')
        for tgt in conn.execute(
            "SELECT id FROM files WHERE path LIKE 'src/%' "
            "AND (name = ? OR name = ?)",
            (base + '.cpp', base + '.h')
        ):
            conn.execute(
                "INSERT INTO edges (source_file, target_file, source_name, "
                "target_name, edge_type) VALUES (?,?,?,?,?)",
                (test_fid, tgt[0], test_name, base, 'tests')
            )
            count += 1

    conn.commit()
    by_type = conn.execute(
        "SELECT edge_type, COUNT(*) FROM edges GROUP BY edge_type "
        "ORDER BY COUNT(*) DESC"
    ).fetchall()
    type_str = ', '.join(f"{t}: {n}" for t, n in by_type)
    print(f"  Built {count} edges ({type_str})")


def scan_qt_connections(conn):
    """Scan for Qt connect() calls."""
    rows = conn.execute(
        "SELECT id, path FROM files WHERE lang = 'cpp' AND ext = '.cpp'"
    ).fetchall()
    count = 0

    for file_id, path in rows:
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        for m in RE_CONNECT.finditer(content):
            sender = m.group(1)
            sig_class = m.group(2)
            signal = m.group(3)
            receiver = m.group(4)
            slot_class = m.group(5)
            slot = m.group(6)
            line_num = content[:m.start()].count('\n') + 1
            conn.execute(
                "INSERT INTO qt_connections "
                "(file_id, sender, signal_name, receiver, slot_name, line_num) "
                "VALUES (?,?,?,?,?,?)",
                (file_id, f"{sender} ({sig_class})", signal,
                 f"{receiver} ({slot_class})", slot, line_num)
            )
            count += 1

    conn.commit()
    print(f"  Found {count} Qt signal/slot connections")


def scan_services(conn):
    """Scan for ServiceRegistry register/resolve calls."""
    rows = conn.execute(
        "SELECT id, path FROM files WHERE lang = 'cpp'"
    ).fetchall()
    count = 0

    for file_id, path in rows:
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        for m in RE_SERVICE_REG.finditer(content):
            line_num = content[:m.start()].count('\n') + 1
            conn.execute(
                "INSERT INTO services (file_id, service_key, line_num, reg_type) "
                "VALUES (?,?,?,?)",
                (file_id, m.group(1), line_num, 'register')
            )
            count += 1

        for m in RE_SERVICE_RES.finditer(content):
            line_num = content[:m.start()].count('\n') + 1
            conn.execute(
                "INSERT INTO services (file_id, service_key, line_num, reg_type) "
                "VALUES (?,?,?,?)",
                (file_id, m.group(1), line_num, 'resolve')
            )
            count += 1

    conn.commit()
    print(f"  Found {count} service registrations/resolutions")


def scan_test_cases(conn):
    """Scan for QTest test methods."""
    rows = conn.execute(
        "SELECT id, path FROM files WHERE path LIKE 'tests/%' AND ext = '.cpp'"
    ).fetchall()
    count = 0

    for file_id, path in rows:
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        test_class = ''
        cm = RE_QTEST_MAIN.search(content)
        if cm:
            test_class = cm.group(1)
        else:
            cls = conn.execute(
                "SELECT name FROM classes WHERE file_id = ? LIMIT 1", (file_id,)
            ).fetchone()
            if cls:
                test_class = cls[0]

        for m in RE_TEST_SLOT.finditer(content):
            line_num = content[:m.start()].count('\n') + 1
            conn.execute(
                "INSERT INTO test_cases "
                "(file_id, test_class, test_method, line_num) VALUES (?,?,?,?)",
                (file_id, test_class, m.group(1), line_num)
            )
            count += 1

    conn.commit()
    print(f"  Found {count} test cases")


def scan_cmake_targets(conn):
    """Scan CMakeLists.txt files for build targets."""
    count = 0

    for scan_dir in SCAN_DIRS:
        full_dir = ROOT / scan_dir
        if not full_dir.exists():
            continue
        for root_path, dirs, filenames in os.walk(full_dir):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for fname in filenames:
                if fname != 'CMakeLists.txt' and not fname.endswith('.cmake'):
                    continue
                fpath = Path(root_path) / fname
                try:
                    content = fpath.read_text(encoding='utf-8', errors='replace')
                except Exception:
                    continue

                rel_path = str(fpath.relative_to(ROOT)).replace('\\', '/')
                row = conn.execute(
                    "SELECT id FROM files WHERE path = ?", (rel_path,)
                ).fetchone()
                file_id = row[0] if row else None

                for m in RE_CMAKE_ADD_EXEC.finditer(content):
                    line_num = content[:m.start()].count('\n') + 1
                    conn.execute(
                        "INSERT INTO cmake_targets "
                        "(file_id, target_name, target_type, line_num) VALUES (?,?,?,?)",
                        (file_id, m.group(1), 'executable', line_num)
                    )
                    count += 1

                for m in RE_CMAKE_ADD_LIB.finditer(content):
                    line_num = content[:m.start()].count('\n') + 1
                    ttype = 'plugin' if 'MODULE' in content[m.start():m.start()+100] else 'library'
                    conn.execute(
                        "INSERT INTO cmake_targets "
                        "(file_id, target_name, target_type, line_num) VALUES (?,?,?,?)",
                        (file_id, m.group(1), ttype, line_num)
                    )
                    count += 1

                for m in RE_CMAKE_ADD_TEST.finditer(content):
                    line_num = content[:m.start()].count('\n') + 1
                    conn.execute(
                        "INSERT INTO cmake_targets "
                        "(file_id, target_name, target_type, line_num) VALUES (?,?,?,?)",
                        (file_id, m.group(1), 'test', line_num)
                    )
                    count += 1

    # Also scan root CMakeLists.txt
    root_cmake = ROOT / 'CMakeLists.txt'
    if root_cmake.exists():
        try:
            content = root_cmake.read_text(encoding='utf-8', errors='replace')
            for m in RE_CMAKE_ADD_EXEC.finditer(content):
                line_num = content[:m.start()].count('\n') + 1
                conn.execute(
                    "INSERT INTO cmake_targets "
                    "(file_id, target_name, target_type, line_num) VALUES (?,?,?,?)",
                    (None, m.group(1), 'executable', line_num)
                )
                count += 1
        except Exception:
            pass

    conn.commit()
    print(f"  Found {count} CMake targets")


def build_fts_index(conn):
    """Populate FTS5 index for fast text search across all symbols."""
    try:
        conn.execute("SELECT * FROM fts_index LIMIT 0")
    except Exception:
        print("  FTS5 not available, skipping")
        return

    count = 0

    # Index files with summaries
    for row in conn.execute(
        "SELECT f.path, fs.summary, fs.category "
        "FROM files f JOIN file_summaries fs ON f.id = fs.file_id"
    ):
        conn.execute(
            "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
            "VALUES (?,?,?,?,?)",
            (Path(row[0]).stem, row[0], row[0], 'file', row[1])
        )
        count += 1

    # Index classes
    for row in conn.execute(
        "SELECT c.name, c.bases, f.path FROM classes c "
        "JOIN files f ON c.file_id = f.id"
    ):
        conn.execute(
            "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
            "VALUES (?,?,?,?,?)",
            (row[0], row[0], row[2], 'class',
             f"bases: {row[1]}" if row[1] else '')
        )
        count += 1

    # Index functions
    for row in conn.execute(
        "SELECT fi.name, fi.qualified_name, f.path "
        "FROM function_index fi JOIN files f ON fi.file_id = f.id"
    ):
        conn.execute(
            "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
            "VALUES (?,?,?,?,?)",
            (row[0], row[1], row[2], 'function', '')
        )
        count += 1

    # Index services
    for row in conn.execute(
        "SELECT s.service_key, s.reg_type, f.path "
        "FROM services s JOIN files f ON s.file_id = f.id"
    ):
        conn.execute(
            "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
            "VALUES (?,?,?,?,?)",
            (row[0], row[0], row[2], 'service', row[1])
        )
        count += 1

    # Index test cases
    for row in conn.execute(
        "SELECT tc.test_method, tc.test_class, f.path "
        "FROM test_cases tc JOIN files f ON tc.file_id = f.id"
    ):
        conn.execute(
            "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
            "VALUES (?,?,?,?,?)",
            (row[0], f"{row[1]}.{row[0]}", row[2], 'test', '')
        )
        count += 1

    conn.commit()
    print(f"  FTS5 index: {count} entries")


def print_summary(conn):
    """Print a summary of the code graph."""
    print("\n" + "=" * 70)
    print("  EXORCIST CODE GRAPH SUMMARY")
    print("=" * 70)

    # File stats
    total = conn.execute("SELECT COUNT(*) FROM files").fetchone()[0]
    by_lang = conn.execute(
        "SELECT lang, COUNT(*), SUM(lines) FROM files GROUP BY lang ORDER BY COUNT(*) DESC"
    ).fetchall()
    print(f"\n  Total files: {total}")
    print(f"  {'Language':<15} {'Files':>8} {'Lines':>10}")
    print(f"  {'-'*15} {'-'*8} {'-'*10}")
    for lang, cnt, lines in by_lang:
        print(f"  {lang:<15} {cnt:>8} {lines:>10}")

    # Directory stats
    print(f"\n  Top directories:")
    dirs = conn.execute(
        "SELECT dir, COUNT(*), SUM(lines) FROM files GROUP BY dir ORDER BY COUNT(*) DESC LIMIT 20"
    ).fetchall()
    for d, cnt, lines in dirs:
        print(f"    {d:<50} {cnt:>5} files  {lines:>8} lines")

    # Class stats
    total_cls = conn.execute("SELECT COUNT(*) FROM classes").fetchone()[0]
    ifaces = conn.execute("SELECT COUNT(*) FROM classes WHERE is_interface = 1").fetchone()[0]
    with_impl = conn.execute("SELECT COUNT(*) FROM classes WHERE has_impl = 1").fetchone()[0]
    no_impl = conn.execute(
        "SELECT COUNT(*) FROM classes WHERE has_impl = 0 AND is_interface = 0 "
        "AND lang = 'cpp'"
    ).fetchone()[0]
    print(f"\n  Classes: {total_cls} total, {ifaces} interfaces, {with_impl} with .cpp, {no_impl} header-only")

    # Feature status
    print(f"\n  Feature Status:")
    print(f"  {'Feature':<25} {'Status':<15} {'Header':<45} {'Source'}")
    print(f"  {'-'*25} {'-'*15} {'-'*45} {'-'*30}")
    feats = conn.execute(
        "SELECT name, status, header_file, source_file FROM features ORDER BY status, name"
    ).fetchall()
    for name, status, hdr, src in feats:
        hdr_short = hdr[:43] if hdr else "-"
        src_short = src[:28] if src else "-"
        icon = {"implemented": "[OK]", "header-only": "[HO]", "stub": "[ST]",
                "missing": "[--]", "source-only": "[SO]"}.get(status, "[??]")
        print(f"  {name:<25} {icon} {status:<12} {hdr_short:<45} {src_short}")

    # Include graph stats
    inc_count = conn.execute("SELECT COUNT(*) FROM includes").fetchone()[0]
    impl_count = conn.execute("SELECT COUNT(*) FROM implementations").fetchone()[0]
    print(f"\n  Include edges: {inc_count}")
    print(f"  Implementation mappings: {impl_count}")

    # Subsystem summary
    print(f"\n  Subsystem Graph (src/ only):")
    print(f"  {'Subsystem':<20} {'Files':>6} {'Lines':>8} {'Classes':>8} {'H-Only':>7} {'Ifaces':>7}")
    print(f"  {'-'*20} {'-'*6} {'-'*8} {'-'*8} {'-'*7} {'-'*7}")
    subsys = conn.execute(
        "SELECT name, file_count, line_count, class_count, h_only_count, interface_count "
        "FROM subsystems ORDER BY line_count DESC"
    ).fetchall()
    for s in subsys:
        print(f"  {s[0]:<20} {s[1]:>6} {s[2]:>8} {s[3]:>8} {s[4]:>7} {s[5]:>7}")

    # Top subsystem dependencies
    print(f"\n  Top Subsystem Dependencies:")
    deps = conn.execute(
        "SELECT from_subsystem, to_subsystem, edge_count "
        "FROM subsystem_deps ORDER BY edge_count DESC LIMIT 25"
    ).fetchall()
    for d in deps:
        print(f"    {d[0]:<18} -> {d[1]:<18} ({d[2]} edges)")

    # Class inheritance stats
    ref_count = conn.execute("SELECT COUNT(*) FROM class_refs").fetchone()[0]
    print(f"\n  Class cross-references: {ref_count}")

    # Feature status summary
    for status in ["implemented", "header-only", "stub", "missing"]:
        cnt = conn.execute(
            "SELECT COUNT(*) FROM features WHERE status = ?", (status,)
        ).fetchone()[0]
        print(f"    {status:15s}: {cnt}")

    print("\n" + "=" * 70)

    # ── Enhanced analysis summary ─────────────────────────────────────────
    call_edges = conn.execute(
        "SELECT COUNT(*) FROM call_graph"
    ).fetchone()[0]
    xml_binds = conn.execute(
        "SELECT COUNT(*) FROM xml_bindings"
    ).fetchone()[0]
    xml_resolved = conn.execute(
        "SELECT COUNT(*) FROM xml_bindings WHERE binding_type != 'unresolved'"
    ).fetchone()[0]
    cfg_issues = conn.execute(
        "SELECT COUNT(*) FROM config_issues"
    ).fetchone()[0]
    cfg_errors = conn.execute(
        "SELECT COUNT(*) FROM config_issues WHERE severity = 'error'"
    ).fetchone()[0]
    rt_eps = conn.execute(
        "SELECT COUNT(*) FROM runtime_entrypoints"
    ).fetchone()[0]
    rt_dynamic = conn.execute(
        "SELECT COUNT(*) FROM runtime_entrypoints WHERE loader_type = 'dynamic'"
    ).fetchone()[0]
    typos_only = conn.execute(
        "SELECT COUNT(*) FROM symbol_aliases WHERE alias_type IN ('typo','likely_typo')"
    ).fetchone()[0]
    all_aliases = conn.execute(
        "SELECT COUNT(*) FROM symbol_aliases"
    ).fetchone()[0]
    dead_funcs = conn.execute(
        "SELECT COUNT(*) FROM reachability WHERE is_reachable = 0 AND dead_reason = 'no-caller'"
    ).fetchone()[0]
    total_funcs = conn.execute(
        "SELECT COUNT(*) FROM reachability"
    ).fetchone()[0]
    top_hotspots = conn.execute(
        "SELECT f.path, h.hotspot_score, h.risk_factors "
        "FROM hotspot_scores h JOIN files f ON h.file_id = f.id "
        "ORDER BY h.hotspot_score DESC LIMIT 8"
    ).fetchall()

    print(f"\n  Enhanced Analysis:")
    print(f"    {'Call graph edges:':<30} {call_edges}")
    print(f"    {'XML bindings:':<30} {xml_binds}  "
          f"(resolved: {xml_resolved}, unresolved: {xml_binds - xml_resolved})")
    print(f"    {'Config issues:':<30} {cfg_issues}  (errors: {cfg_errors})")
    print(f"    {'Runtime load entrypoints:':<30} {rt_eps}  "
          f"(dynamic/variable: {rt_dynamic})")
    print(f"    {'Symbol aliases:':<30} {all_aliases}  (typos: {typos_only})")
    print(f"    {'Potentially dead functions:':<30} {dead_funcs} / {total_funcs}")

    if top_hotspots:
        print(f"\n  Top Hotspot Files (coupling \u00d7 no-tests \u00d7 crash-risk):")
        print(f"  {'File':<45} {'Score':>6}  Risk factors")
        print(f"  {'-'*45} {'-'*6}  {'-'*40}")
        for p, sc, rf in top_hotspots:
            short = p.split('/')[-1]
            print(f"  {short:<45} {sc:>6.1f}  {(rf or '')[:55]}")

    if typos_only:
        print(f"\n  Likely Typos / Rename Drift (edit-distance \u2264 2):")
        rows = conn.execute(
            "SELECT sa.symbol_a, sa.symbol_b, sa.edit_distance, "
            "fa.path, fb.path "
            "FROM symbol_aliases sa "
            "LEFT JOIN files fa ON sa.file_a = fa.id "
            "LEFT JOIN files fb ON sa.file_b = fb.id "
            "WHERE sa.alias_type IN ('typo','likely_typo') "
            "ORDER BY sa.edit_distance, sa.symbol_a LIMIT 20"
        ).fetchall()
        for a, b, dist, pa, pb in rows:
            fa_short = (pa or '?').split('/')[-1]
            fb_short = (pb or '?').split('/')[-1]
            print(f"    {a:<30} vs  {b:<30}  dist={dist}  "
                  f"({fa_short} / {fb_short})")

    # ── AI intelligence layer stats ────────────────────────────────────────
    _ai_tables = [
        ('function_summary',     'Function summaries'),
        ('symbol_slices',        'Symbol slices'),
        ('analysis_scores',      'Analysis scores'),
        ('ai_tasks',             'AI tasks (pending)'),
        ('git_delta',            'Git delta entries'),
        ('optimization_patterns','Optimization patterns'),
    ]
    ai_found = False
    for tbl, label in _ai_tables:
        try:
            n = conn.execute(f"SELECT COUNT(*) FROM {tbl}").fetchone()[0]
            if not ai_found:
                print(f"\n  AI Intelligence Layer:")
                ai_found = True
            print(f"    {label:<28s}: {n}")
        except Exception:
            pass

    try:
        top_bns = conn.execute(
            "SELECT symbol_name, overall_priority, reasons "
            "FROM analysis_scores ORDER BY overall_priority DESC LIMIT 8"
        ).fetchall()
        if top_bns:
            print(f"\n  Top Bottleneck Candidates:")
            print(f"  {'Symbol':<45} {'Priority':>8}  Reasons")
            print(f"  {'-'*45} {'-'*8}  {'-'*40}")
            for sym, pri, rs in top_bns:
                short = sym.split('::')[-1] if '::' in sym else sym
                print(f"  {short:<45} {pri:>8}  {(rs or '')[:45]}")
    except Exception:
        pass

    # Actionable gap report
    print_gap_report(conn)


def print_gap_report(conn):
    """Print an actionable report of implementation gaps that need work."""
    print("\n" + "=" * 70)
    print("  ACTIONABLE GAP REPORT")
    print("=" * 70)

    # ── 1. Header-only features (tracked features missing .cpp) ──
    ho_feats = conn.execute(
        "SELECT name, header_file, class_name FROM features WHERE status = 'header-only' ORDER BY name"
    ).fetchall()
    if ho_feats:
        print(f"\n  [HO] Features needing .cpp implementation ({len(ho_feats)}):")
        print(f"  {'Feature':<25} {'Header':<50} {'Action'}")
        print(f"  {'-'*25} {'-'*50} {'-'*35}")
        for name, hdr, cls in ho_feats:
            base = hdr.rsplit(".", 1)[0] if hdr else "?"
            cpp = base + ".cpp"
            print(f"  {name:<25} {(hdr or '-'):<50} Create {cpp}")

    # ── 2. Stub features (have .cpp but minimal code) ──
    stub_feats = conn.execute(
        "SELECT name, header_file, source_file FROM features WHERE status = 'stub' ORDER BY name"
    ).fetchall()
    if stub_feats:
        print(f"\n  [ST] Stub features needing full implementation ({len(stub_feats)}):")
        for name, hdr, src in stub_feats:
            print(f"    {name:<25} Flesh out {src}")

    # ── 3. Missing features ──
    missing_feats = conn.execute(
        "SELECT name FROM features WHERE status = 'missing' ORDER BY name"
    ).fetchall()
    if missing_feats:
        print(f"\n  [--] Missing features ({len(missing_feats)}):")
        for (name,) in missing_feats:
            print(f"    {name:<25} Needs design + implementation from scratch")

    # ── 4. QObject header-only classes (MUST have .cpp for moc) ──
    qobj_ho = conn.execute("""
        SELECT c.name, f.path FROM classes c
        JOIN files f ON c.file_id = f.id
        WHERE c.has_impl = 0 AND c.is_interface = 0
        AND f.has_qobject = 1 AND f.ext = '.h'
        AND f.path LIKE 'src/%'
        AND c.lang = 'cpp'
        ORDER BY f.path
    """).fetchall()
    if qobj_ho:
        print(f"\n  [!!] QObject classes without .cpp (moc issues) ({len(qobj_ho)}):")
        for cls, path in qobj_ho:
            base = path.rsplit(".", 1)[0]
            print(f"    {cls:<35} {path}  -> Create {base}.cpp")

    # ── 5. Non-trivial header-only classes (not data structs) ──
    # Heuristic: class name > 10 chars, not a struct, not an interface,
    # has Q_OBJECT or has methods with bodies
    big_ho = conn.execute("""
        SELECT c.name, f.path FROM classes c
        JOIN files f ON c.file_id = f.id
        WHERE c.has_impl = 0 AND c.is_interface = 0 AND c.is_struct = 0
        AND c.lang = 'cpp' AND f.ext = '.h'
        AND f.path LIKE 'src/%'
        AND length(c.name) > 12
        AND f.has_qobject = 0
        ORDER BY f.path
    """).fetchall()
    if big_ho:
        print(f"\n  [HO] Large header-only classes (likely need .cpp) ({len(big_ho)}):")
        for cls, path in big_ho:
            print(f"    {cls:<35} {path}")

    # ── 6. Interfaces without any implementation ──
    orphan_ifaces = conn.execute("""
        SELECT c.name, f.path FROM classes c
        JOIN files f ON c.file_id = f.id
        WHERE c.is_interface = 1
        AND f.path LIKE 'src/%'
        AND NOT EXISTS (
            SELECT 1 FROM class_refs cr
            WHERE cr.to_class = c.name AND cr.ref_type = 'inherits'
        )
        ORDER BY f.path
    """).fetchall()
    if orphan_ifaces:
        print(f"\n  [IF] Interfaces with no known implementors ({len(orphan_ifaces)}):")
        for cls, path in orphan_ifaces:
            print(f"    {cls:<35} {path}")

    # ── 7. Subsystem health (H-Only ratio) ──
    unhealthy = conn.execute("""
        SELECT name, class_count, h_only_count, interface_count,
               CASE WHEN class_count > 0
                    THEN ROUND(100.0 * h_only_count / class_count, 1)
                    ELSE 0 END AS ho_pct
        FROM subsystems
        WHERE class_count > 3 AND h_only_count > 2
        ORDER BY ho_pct DESC
    """).fetchall()
    if unhealthy:
        print(f"\n  [!!] Subsystems with high header-only ratio:")
        print(f"  {'Subsystem':<20} {'Classes':>8} {'H-Only':>8} {'%':>8}")
        print(f"  {'-'*20} {'-'*8} {'-'*8} {'-'*8}")
        for name, cls_c, ho_c, _, pct in unhealthy:
            print(f"  {name:<20} {cls_c:>8} {ho_c:>8} {pct:>7.1f}%")

    # Summary
    total_gaps = len(ho_feats) + len(stub_feats) + len(missing_feats) + len(qobj_ho)
    print(f"\n  TOTAL actionable gaps: {total_gaps}")
    print(f"    Features needing .cpp:   {len(ho_feats)}")
    print(f"    Stubs to flesh out:      {len(stub_feats)}")
    print(f"    Missing features:        {len(missing_feats)}")
    print(f"    QObject without .cpp:    {len(qobj_ho)}")
    print("=" * 70)


# ══════════════════════════════════════════════════════════════════════════
# Enhanced analyses: call graph, XML bindings, runtime loaders,
#                    symbol aliases, reachability, hotspot scores
# ══════════════════════════════════════════════════════════════════════════

def _levenshtein(a, b):
    """Wagner-Fischer edit distance between two strings."""
    if a == b:
        return 0
    if len(a) < len(b):
        a, b = b, a
    if not b:
        return len(a)
    prev = list(range(len(b) + 1))
    for ca in a:
        curr = [prev[0] + 1]
        for j, cb in enumerate(b):
            curr.append(min(prev[j + 1] + 1, curr[j] + 1,
                            prev[j] + (0 if ca == cb else 1)))
        prev = curr
    return prev[-1]


def build_call_graph(conn):
    """Build a function-level call graph by scanning every function body.

    For each function in function_index, scan its source lines for calls to
    other known functions and record an edge in call_graph.
    """
    # All known function names (>= 4 chars, skip control-flow keywords)
    known_names = set()
    name_to_fid = {}
    for name, fid in conn.execute(
        "SELECT name, file_id FROM function_index WHERE length(name) >= 4"
    ):
        if name not in _SKIP_KEYWORDS:
            known_names.add(name)
            name_to_fid.setdefault(name, fid)

    # Group function records by source file to read each file once
    file_funcs = {}
    for fid, qname, fname, start, end in conn.execute(
        "SELECT file_id, qualified_name, name, start_line, end_line "
        "FROM function_index WHERE length(name) >= 4"
    ):
        file_funcs.setdefault(fid, []).append((qname, fname, start, end))

    file_paths = dict(conn.execute(
        "SELECT id, path FROM files WHERE lang = 'cpp'"
    ))

    count = 0
    for file_id, funcs in file_funcs.items():
        path = file_paths.get(file_id)
        if not path:
            continue
        try:
            src_lines = (ROOT / path).read_text(
                encoding='utf-8', errors='replace'
            ).split('\n')
        except Exception:
            continue

        for qname, fname, start, end in funcs:
            body = '\n'.join(src_lines[max(0, start - 1):end])
            seen_callees = set()
            for m in RE_FUNC_CALL.finditer(body):
                callee = m.group(1)
                if (callee in known_names
                        and callee != fname
                        and callee not in seen_callees
                        and callee not in _SKIP_KEYWORDS):
                    seen_callees.add(callee)
                    line_off = body[:m.start()].count('\n')
                    conn.execute(
                        "INSERT INTO call_graph "
                        "(caller_file, caller_func, callee_func, callee_file, line_num) "
                        "VALUES (?,?,?,?,?)",
                        (file_id, qname, callee,
                         name_to_fid.get(callee), start + line_off)
                    )
                    count += 1

    conn.commit()
    print(f"  Built {count} call graph edges")


def scan_xml_bindings(conn):
    """Bind XML element/attribute values to C++ symbols; detect config issues.

    For every .xml file:
      - Resolves attribute values and text content to known C++ class/function names.
      - Flags duplicate id= / name= attributes (duplicate_id, duplicate_name).
      - Records unresolved type=/kind=/handler=/state= values for manual review.
    """
    # Collect known C++ symbols -> file_id
    known_syms = {}
    for name, fid in conn.execute("SELECT name, file_id FROM classes"):
        known_syms[name] = fid
    for name, fid in conn.execute(
        "SELECT DISTINCT name, file_id FROM function_index WHERE is_method = 0"
    ):
        known_syms.setdefault(name, fid)

    xml_files = conn.execute(
        "SELECT id, path FROM files WHERE ext = '.xml'"
    ).fetchall()

    _TYPED_ATTRS = frozenset({
        'type', 'kind', 'handler', 'state', 'action',
        'class', 'baseClass', 'transition', 'event', 'command',
    })

    bind_count = 0
    issue_count = 0

    for file_id, path in xml_files:
        full_path = ROOT / path
        try:
            tree = ET.parse(str(full_path))
            root_elem = tree.getroot()
        except Exception:
            continue

        seen_ids = {}    # id_value -> tag
        seen_names = set()

        def _walk(elem):
            nonlocal bind_count, issue_count
            tag = elem.tag.split('}')[-1]   # strip XML namespace
            attribs = elem.attrib

            # Duplicate id detection
            eid = attribs.get('id') or attribs.get('Id') or attribs.get('ID')
            if eid:
                if eid in seen_ids:
                    conn.execute(
                        "INSERT INTO config_issues "
                        "(xml_file, issue_type, element, attribute, value, message, severity) "
                        "VALUES (?,?,?,?,?,?,?)",
                        (file_id, 'duplicate_id', tag, 'id', eid,
                         f"Duplicate id='{eid}' in <{tag}>", 'error')
                    )
                    issue_count += 1
                seen_ids[eid] = tag

            # Duplicate name detection
            ename = attribs.get('name') or attribs.get('Name')
            if ename:
                key = f"{tag}:{ename}"
                if key in seen_names:
                    conn.execute(
                        "INSERT INTO config_issues "
                        "(xml_file, issue_type, element, attribute, value, message, severity) "
                        "VALUES (?,?,?,?,?,?,?)",
                        (file_id, 'duplicate_name', tag, 'name', ename,
                         f"Duplicate name='{ename}' in <{tag}>", 'warning')
                    )
                    issue_count += 1
                seen_names.add(key)

            # Bind attribute values to known C++ symbols
            for attr, val in attribs.items():
                if val in known_syms:
                    conn.execute(
                        "INSERT INTO xml_bindings "
                        "(xml_file, element, attribute, value, "
                        "bound_symbol, bound_file, binding_type) VALUES (?,?,?,?,?,?,?)",
                        (file_id, tag, attr, val, val,
                         known_syms[val], 'symbol_ref')
                    )
                    bind_count += 1
                elif attr in _TYPED_ATTRS:
                    # Record unresolved type/handler references for review
                    conn.execute(
                        "INSERT INTO xml_bindings "
                        "(xml_file, element, attribute, value, "
                        "bound_symbol, binding_type) VALUES (?,?,?,?,?,?)",
                        (file_id, tag, attr, val, '', 'unresolved')
                    )
                    bind_count += 1

            # Text body may reference a symbol
            if elem.text and elem.text.strip():
                txt = elem.text.strip()
                if txt in known_syms:
                    conn.execute(
                        "INSERT INTO xml_bindings "
                        "(xml_file, element, attribute, value, "
                        "bound_symbol, bound_file, binding_type) VALUES (?,?,?,?,?,?,?)",
                        (file_id, tag, '_text', txt, txt,
                         known_syms[txt], 'text_ref')
                    )
                    bind_count += 1

            for child in elem:
                _walk(child)

        _walk(root_elem)

    conn.commit()
    print(f"  XML analysis: {bind_count} bindings, {issue_count} config issues "
          f"in {len(xml_files)} XML files")


def scan_runtime_entrypoints(conn):
    """Detect where the program loads files, plugins, or scripts at runtime."""
    patterns = [
        (re.compile(r'QPluginLoader\b[^;{]*?["\']([^"\']{3,})["\']'),
         'QPluginLoader'),
        (re.compile(r'QFile\s+\w+\s*\(\s*["\']([^"\']{3,})["\']'),
         'QFile'),
        (re.compile(r'\.load\s*\(\s*["\']([^"\']{3,})["\']'),
         'load()'),
        (re.compile(
            r'\b(?:loadFile|loadFrom|readFrom|loadPlugin|loadScript|loadLua)'
            r'\s*\(\s*["\']([^"\']{3,})["\']'),
         'loader-func'),
        # Variable-based loads: QPluginLoader p(m_path)
        (re.compile(
            r'(?:QPluginLoader|loadPlugin|loadLua|loadScript)\s*[\({]\s*'
            r'(m_\w+|\w+[Pp]ath|\w+[Ff]ile)\b'),
         'dynamic'),
    ]

    count = 0
    for file_id, path in conn.execute(
        "SELECT id, path FROM files WHERE lang = 'cpp'"
    ):
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue
        for pat, label in patterns:
            for m in pat.finditer(content):
                line_num = content[:m.start()].count('\n') + 1
                conn.execute(
                    "INSERT INTO runtime_entrypoints "
                    "(file_id, loader_type, target, line_num) VALUES (?,?,?,?)",
                    (file_id, label, m.group(1), line_num)
                )
                count += 1

    conn.commit()
    print(f"  Found {count} runtime loading entrypoints")


def build_symbol_aliases(conn):
    """Detect similarly-named symbols (potential typos or rename drift).

    Uses Levenshtein edit distance grouped by first-3-char prefix to keep
    the comparison O(n) in practice while catching 1-2 char differences.
    """
    # Class names from project source
    classes = list(conn.execute(
        "SELECT c.name, f.id FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE length(c.name) >= 5 "
        "AND (f.path LIKE 'src/%' OR f.path LIKE 'plugins/%')"
    ))
    # Free functions (not methods)
    funcs = list(conn.execute(
        "SELECT DISTINCT fi.name, fi.file_id FROM function_index fi "
        "WHERE fi.is_method = 0 AND length(fi.name) >= 6"
    ))

    count = 0

    def _check_group(items):
        nonlocal count
        n = len(items)
        for i in range(n):
            a, fa = items[i]
            for j in range(i + 1, n):
                b, fb = items[j]
                if a == b or fa == fb:
                    continue
                if abs(len(a) - len(b)) > 3:
                    continue
                dist = _levenshtein(a, b)
                if dist == 0 or dist > 3:
                    continue
                if dist == 1:
                    atype = 'typo'
                elif dist == 2 and len(a) > 7:
                    atype = 'likely_typo'
                else:
                    atype = 'similar'
                conn.execute(
                    "INSERT INTO symbol_aliases "
                    "(symbol_a, file_a, symbol_b, file_b, edit_distance, alias_type) "
                    "VALUES (?,?,?,?,?,?)",
                    (a, fa, b, fb, dist, atype)
                )
                count += 1

    # Group by first 3 lowercase chars to limit O(n²) scope
    cls_groups = {}
    for name, fid in classes:
        cls_groups.setdefault(name[:3].lower(), []).append((name, fid))
    for grp in cls_groups.values():
        _check_group(grp)

    fn_groups = {}
    for name, fid in funcs:
        fn_groups.setdefault(name[:3].lower(), []).append((name, fid))
    for grp in fn_groups.values():
        _check_group(grp)

    conn.commit()
    typos = conn.execute(
        "SELECT COUNT(*) FROM symbol_aliases "
        "WHERE alias_type IN ('typo', 'likely_typo')"
    ).fetchone()[0]
    print(f"  Found {count} symbol aliases ({typos} potential typos)")


def build_reachability(conn):
    """BFS reachability analysis starting from known program entrypoints.

    Marks every function in function_index as reachable or unreachable.
    Functions with no path from any entrypoint are flagged as 'no-caller'.
    Qt slots (on*), tests, and plugin lifecycle methods are treated as
    implicitly reachable (not flagged dead).
    """
    ENTRY_NAMES = frozenset({
        'main', 'initialize', 'shutdown', 'createView', 'info',
        'execute', 'run', 'start', 'stop', 'load', 'unload',
        'create', 'instance', 'getInstance', 'setup', 'init',
        'cleanup', 'reset', 'update', 'process',
    })

    # Build adjacency: bare_caller -> {bare_callee}
    adj = {}
    for caller, callee in conn.execute(
        "SELECT caller_func, callee_func FROM call_graph"
    ):
        bare_caller = caller.split('::')[-1]
        adj.setdefault(bare_caller, set()).add(callee)

    # All functions: qualified_name -> file_id
    all_funcs = dict(conn.execute(
        "SELECT qualified_name, file_id FROM function_index"
    ))

    # bare_name -> [qualified_names]
    bare_to_qual = {}
    for qn in all_funcs:
        bare_to_qual.setdefault(qn.split('::')[-1], []).append(qn)

    # BFS
    reachable = set()
    queue = []
    for qn in all_funcs:
        if qn.split('::')[-1] in ENTRY_NAMES:
            if qn not in reachable:
                reachable.add(qn)
                queue.append(qn)

    head = 0
    while head < len(queue):
        curr = queue[head]
        head += 1
        for callee_bare in adj.get(curr.split('::')[-1], set()):
            for qn in bare_to_qual.get(callee_bare, []):
                if qn not in reachable:
                    reachable.add(qn)
                    queue.append(qn)

    # Build rows
    rows = []
    for qn, fid in all_funcs.items():
        bare = qn.split('::')[-1]
        if qn in reachable:
            rows.append((fid, qn, 'function', 1, 'entry-chain', ''))
        elif bare.startswith('test') or bare.startswith('Test'):
            rows.append((fid, qn, 'function', 1, 'test-runner', ''))
        elif bare.startswith('on') or bare.startswith('On'):
            # Qt slots are reachable via signal/slot (not in call graph)
            rows.append((fid, qn, 'function', 1, 'slot-candidate', ''))
        else:
            rows.append((fid, qn, 'function', 0, '', 'no-caller'))

    conn.executemany(
        "INSERT INTO reachability "
        "(file_id, symbol, symbol_type, is_reachable, reachable_via, dead_reason) "
        "VALUES (?,?,?,?,?,?)",
        rows
    )
    conn.commit()
    dead = sum(1 for r in rows if r[3] == 0)
    print(f"  Reachability: {len(reachable)} reachable, "
          f"{dead} potentially unreachable functions")


def build_hotspot_scores(conn):
    """Score every C++ source file on three axes:
      - coupling_score   : sum of include in-degree + out-degree (normalised)
      - coverage_gap     : 1.0 if no test edge points here, else 0.0
      - crash_risk_score : count of crash-prone patterns (bare new/delete,
                           unguarded casts, FIXME/HACK, heavy pointer use)

    hotspot_score = (coupling*0.35 + coverage_gap*0.40 + crash_risk*0.25) * 100
    """
    CRASH_PATTERNS = [
        (re.compile(r'dynamic_cast<[^>]+>\s*\([^)]+\)\s*->'), 'unguarded-cast'),
        (re.compile(r'\bnew\s+\w+'),                           'bare-new'),
        (re.compile(r'\bdelete\s+\w+'),                        'bare-delete'),
        (re.compile(r'//\s*(?:FIXME|HACK|XXX)\b'),             'tech-debt'),
        (re.compile(r'\bstatic_cast<\w+\s*\*>'),               'ptr-cast'),
    ]

    count = 0
    for file_id, path in conn.execute(
        "SELECT id, path FROM files WHERE lang = 'cpp' "
        "AND path NOT LIKE 'tests/%'"
    ):
        cin = conn.execute(
            "SELECT COUNT(*) FROM edges "
            "WHERE target_file=? AND edge_type='includes'", (file_id,)
        ).fetchone()[0]
        cout = conn.execute(
            "SELECT COUNT(*) FROM edges "
            "WHERE source_file=? AND edge_type='includes'", (file_id,)
        ).fetchone()[0]
        has_tests = conn.execute(
            "SELECT COUNT(*) FROM edges "
            "WHERE target_file=? AND edge_type='tests'", (file_id,)
        ).fetchone()[0]

        crash_score = 0
        risk_tags = []
        try:
            content = (ROOT / path).read_text(encoding='utf-8', errors='replace')
            raw_deref = len(re.findall(r'\w\s*->', content))
            if raw_deref > 40:
                crash_score += 2
                risk_tags.append(f'heavy-deref:{raw_deref}')
            elif raw_deref > 15:
                crash_score += 1
            for pat, tag in CRASH_PATTERNS:
                hits = len(pat.findall(content))
                if hits:
                    crash_score += min(hits, 3)
                    risk_tags.append(f'{tag}:{hits}')
        except Exception:
            pass

        coupling = cin + cout
        norm_c    = min(coupling / 25.0, 1.0)
        norm_cov  = 0.0 if has_tests else 1.0
        norm_risk = min(crash_score / 10.0, 1.0)
        score     = round(
            (norm_c * 0.35 + norm_cov * 0.40 + norm_risk * 0.25) * 100, 1
        )

        if score > 5 or crash_score > 0:
            conn.execute(
                "INSERT INTO hotspot_scores "
                "(file_id, coupling_in, coupling_out, has_tests, "
                "crash_indicators, hotspot_score, risk_factors) "
                "VALUES (?,?,?,?,?,?,?)",
                (file_id, cin, cout, 1 if has_tests else 0,
                 crash_score, score, ', '.join(risk_tags))
            )
            count += 1

    conn.commit()
    top = conn.execute(
        "SELECT f.path, h.hotspot_score, h.risk_factors "
        "FROM hotspot_scores h JOIN files f ON h.file_id = f.id "
        "ORDER BY h.hotspot_score DESC LIMIT 5"
    ).fetchall()
    print(f"  Scored {count} files; top hotspots:")
    for p, sc, rf in top:
        short = p.split('/')[-1]
        print(f"    {short:<42} score={sc:>5.1f}  {(rf or '')[:55]}")


def build_semantic_tags(conn):
    """Attach semantic/security/performance/audit tags to files, classes, and functions."""
    count = 0
    rows = conn.execute(
        "SELECT id, path, lang, subsystem FROM files ORDER BY path"
    ).fetchall()

    for file_id, path, lang, subsystem in rows:
        try:
            content = (ROOT / path).read_text(encoding="utf-8", errors="replace")
        except Exception:
            content = ""

        for tag_value in _infer_semantic_tags(path, lang, subsystem, content, Path(path).name):
            tag, _, value = tag_value.partition(":")
            _insert_semantic_tag(conn, file_id, "file", path, 0, tag, value, 85, "heuristic", path)
            count += 1

        is_project_owned = path.startswith(("src/", "plugins/", "server/"))
        if is_project_owned:
            metric = conn.execute(
                "SELECT coupling_in, coupling_out, has_tests, crash_indicators, hotspot_score "
                "FROM hotspot_scores WHERE file_id = ?",
                (file_id,)
            ).fetchone()
            if metric:
                has_tests = bool(metric[2])
                crash_indicators = int(metric[3] or 0)
                hotspot_score = float(metric[4] or 0.0)
                _insert_semantic_tag(
                    conn, file_id, "file", path, 0, "audit",
                    "unit-covered" if has_tests else "uncovered", 90, "derived", "hotspot_scores"
                )
                count += 1
                if hotspot_score >= 8.0:
                    _insert_semantic_tag(conn, file_id, "file", path, 0, "performance",
                                         "hot-path", 95, "derived", "hotspot_scores")
                    count += 1
                if crash_indicators > 0:
                    _insert_semantic_tag(conn, file_id, "file", path, 0, "security",
                                         "fragile", 80, "derived", "hotspot_scores")
                    count += 1

            if conn.execute("SELECT COUNT(*) FROM runtime_entrypoints WHERE file_id = ?", (file_id,)).fetchone()[0] > 0:
                _insert_semantic_tag(conn, file_id, "file", path, 0, "runtime",
                                     "entrypoint", 90, "derived", "runtime_entrypoints")
                count += 1
            if conn.execute("SELECT COUNT(*) FROM services WHERE file_id = ?", (file_id,)).fetchone()[0] > 0:
                _insert_semantic_tag(conn, file_id, "file", path, 0, "architecture",
                                     "service-participant", 85, "derived", "services")
                count += 1

        class_rows = conn.execute(
            "SELECT name, line_num FROM classes WHERE file_id = ?",
            (file_id,)
        ).fetchall()
        for name, line_num in class_rows:
            for tag_value in _infer_semantic_tags(path, lang, subsystem, "", name):
                tag, _, value = tag_value.partition(":")
                _insert_semantic_tag(conn, file_id, "class", name, line_num or 0,
                                     tag, value, 75, "heuristic", name)
                count += 1

        func_rows = conn.execute(
            "SELECT qualified_name, start_line FROM function_index WHERE file_id = ?",
            (file_id,)
        ).fetchall()
        for qualified_name, start_line in func_rows:
            for tag_value in _infer_semantic_tags(path, lang, subsystem, "", qualified_name):
                tag, _, value = tag_value.partition(":")
                _insert_semantic_tag(conn, file_id, "function", qualified_name, start_line or 0,
                                     tag, value, 65, "heuristic", qualified_name)
                count += 1

    conn.commit()
    print(f"  Built {count} semantic tags")


# ══════════════════════════════════════════════════════════════════════════
# AI-context optimisation builders (steps 20-26)
# ══════════════════════════════════════════════════════════════════════════

def build_git_delta(conn):
    """Populate git_delta with files touched in the last 20 commits."""
    try:
        result = subprocess.run(
            ['git', 'log', '--name-only', '--format=', '-20'],
            capture_output=True, text=True, cwd=str(ROOT), timeout=15
        )
        if result.returncode != 0:
            print("  git log unavailable, skipping git_delta")
            return

        changed_paths = set()
        for line in result.stdout.splitlines():
            line = line.strip()
            if line:
                changed_paths.add(line.replace('\\', '/'))

        count = 0
        for rel_path in changed_paths:
            row = conn.execute(
                "SELECT id FROM files WHERE path = ?", (rel_path,)
            ).fetchone()
            if not row:
                continue
            file_id = row[0]

            diff_result = subprocess.run(
                ['git', 'diff', 'HEAD~1', '--', rel_path],
                capture_output=True, text=True, cwd=str(ROOT), timeout=10
            )
            snippet = diff_result.stdout[:500] if diff_result.returncode == 0 else ''
            added   = sum(1 for l in snippet.splitlines()
                          if l.startswith('+') and not l.startswith('+++'))
            removed = sum(1 for l in snippet.splitlines()
                          if l.startswith('-') and not l.startswith('---'))
            conn.execute(
                "INSERT OR IGNORE INTO git_delta "
                "(file_id, changed_at, diff_snippet, lines_added, lines_removed) "
                "VALUES (?,datetime('now'),?,?,?)",
                (file_id, snippet[:400], added, removed)
            )
            conn.execute(
                "UPDATE function_index SET recently_modified = 1 WHERE file_id = ?",
                (file_id,)
            )
            count += 1

        conn.commit()
        print(f"  git_delta: {count} recently modified files tracked")
    except FileNotFoundError:
        print("  git not found, skipping git_delta")
    except Exception as e:
        print(f"  git_delta warning: {e}")


def detect_duplicate_functions(conn):
    """Detect functions with identical body hashes (copy-paste / divergent clones)."""
    dupes = conn.execute(
        "SELECT body_hash, COUNT(*) AS cnt "
        "FROM function_index "
        "WHERE body_hash != '' AND length(body_hash) > 4 "
        "GROUP BY body_hash HAVING cnt > 1"
    ).fetchall()

    count = 0
    for body_hash, _ in dupes:
        first = conn.execute(
            "SELECT qualified_name FROM function_index "
            "WHERE body_hash = ? LIMIT 1", (body_hash,)
        ).fetchone()
        group_label = first[0] if first else body_hash[:8]
        conn.execute(
            "UPDATE function_index SET duplicate_group = ? WHERE body_hash = ?",
            (group_label, body_hash)
        )
        count += 1

    conn.commit()
    total = conn.execute(
        "SELECT COUNT(*) FROM function_index WHERE duplicate_group != ''"
    ).fetchone()[0]
    print(f"  Duplicate detection: {count} groups, {total} functions flagged")


def build_symbol_slices(conn):
    """Build symbol slices: signature + up to 12 critical lines per function.

    A 'slice' is a compact, low-token representation of a function that an AI
    can reason about without reading the full body (~80-90% token reduction).
    """
    rows = conn.execute(
        "SELECT fi.file_id, fi.qualified_name, fi.start_line, fi.end_line, f.path "
        "FROM function_index fi JOIN files f ON fi.file_id = f.id "
        "WHERE fi.line_count > 0"
    ).fetchall()

    _CRITICAL_KW = (
        'for ', 'while ', 'if (', 'if(', 'return ',
        '*=', '/=', '+=', '-=', 'assert', 'Q_ASSERT',
        'VERIFY', 'CHECK', 'emit ', '->', 'throw ',
    )
    count = 0
    for file_id, qname, start, end, path in rows:
        try:
            src_lines = (ROOT / path).read_text(
                encoding='utf-8', errors='replace'
            ).split('\n')
        except Exception:
            continue

        body_lines = src_lines[max(0, start - 1):end]
        if not body_lines:
            continue

        signature = body_lines[0].strip()
        critical = []
        for ln in body_lines[1:]:
            stripped = ln.strip()
            if stripped and any(kw in stripped for kw in _CRITICAL_KW):
                critical.append(stripped[:120])
                if len(critical) >= 12:
                    break
        critical_text = '\n'.join(critical)

        slice_tokens = (
            len(signature.split())
            + sum(len(l.split()) for l in critical)
        ) * 2  # rough token estimate (words × 2)
        full_tokens = sum(len(l.split()) for l in body_lines) * 2

        conn.execute(
            "INSERT OR REPLACE INTO symbol_slices "
            "(file_id, symbol, signature, critical_lines, "
            "slice_token_estimate, full_token_estimate) "
            "VALUES (?,?,?,?,?,?)",
            (file_id, qname, signature, critical_text, slice_tokens, full_tokens)
        )
        count += 1

    conn.commit()
    print(f"  Built {count} symbol slices")


def build_function_summaries(conn):
    """Build per-function metadata: inferred category, flags, recently_modified."""
    rows = conn.execute(
        "SELECT fi.file_id, fi.qualified_name, fi.name, "
        "fi.hot_path, fi.ct_sensitive, fi.gpu_candidate, "
        "fi.batchable, fi.recently_modified, fi.body_hash, f.path "
        "FROM function_index fi JOIN files f ON fi.file_id = f.id"
    ).fetchall()

    # Paths touched in git_delta  → recently modified
    recent_paths = set(
        r[0] for r in conn.execute(
            "SELECT DISTINCT f.path FROM git_delta gd "
            "JOIN files f ON gd.file_id = f.id"
        )
    )

    count = 0
    for (file_id, qname, fname, hot, ct, gpu,
         batch, recently_mod, bhash, path) in rows:

        if path in recent_paths:
            recently_mod = 1

        lower = fname.lower()
        if any(k in lower for k in ('test', 'bench', 'mock', 'stub', 'fake')):
            category = 'test'
        elif any(k in lower for k in ('init', 'setup', 'create', 'build', 'construct')):
            category = 'init'
        elif any(k in lower for k in ('verify', 'check', 'validate', 'assert')):
            category = 'validation'
        elif gpu:
            category = 'compute-gpu'
        elif hot:
            category = 'compute-hot'
        elif ct:
            category = 'compute-ct'
        elif batch:
            category = 'compute-batch'
        elif any(k in lower for k in ('parse', 'encode', 'decode', 'serialize')):
            category = 'serialization'
        elif any(k in lower for k in ('get', 'set', 'load', 'store', 'read', 'write')):
            category = 'accessor'
        else:
            category = 'general'

        conn.execute(
            "INSERT OR REPLACE INTO function_summary "
            "(file_id, symbol, qualified_symbol, category, "
            "batchable, gpu_candidate, ct_sensitive, recently_modified, "
            "body_hash, last_updated) "
            "VALUES (?,?,?,?,?,?,?,?,?,datetime('now'))",
            (file_id, fname, qname, category,
             batch, gpu, ct, recently_mod, bhash or '')
        )
        count += 1

    conn.commit()
    print(f"  Built {count} function summaries")


def build_optimization_patterns(conn):
    """Seed the optimization_patterns table with known performance patterns."""
    patterns = [
        ("batch_inversion",
         "Montgomery batch inversion avoids per-element modular inverse",
         "100-1000x vs per-element modinv", "low",
         "batchable=1 AND ct_sensitive=0", "secp256k1_fe_inv_all_var"),
        ("montgomery_ladder",
         "Constant-time scalar mult via Montgomery ladder",
         "CT compliance", "medium",
         "ct_sensitive=1 AND name LIKE '%scalar_mul%'", "secp256k1_ecmult_const"),
        ("CIOS_multiplication",
         "Coarsely Integrated Operand Scanning for modular mult",
         "30-40% vs schoolbook", "medium",
         "name LIKE '%mul%' AND name LIKE '%field%'", "secp256k1_fe_mul"),
        ("precomputed_windowed",
         "Precomputed table with wNAF for fixed-base multiplication",
         "5-10x vs variable-base", "low",
         "name LIKE '%ecmult_gen%'", "secp256k1_ecmult_gen"),
        ("vectorized_sha256",
         "SIMD SHA256 using 4/8-way parallelism",
         "4-8x throughput", "low",
         "name LIKE '%sha256%' AND batchable=1", ""),
        ("straus_shamir",
         "Strauss-Shamir trick for multi-scalar multiplication",
         "2x vs consecutive ecmult", "low",
         "fanin_score > 3 AND name LIKE '%ecmult%'", "secp256k1_ecmult"),
        ("lazy_normalization",
         "Delay field normalization to reduce normalize calls",
         "10-20% in multi-op sequences", "low",
         "name LIKE '%normalize%' OR name LIKE '%reduce%'", ""),
        ("endomorphism_split",
         "GLV endomorphism for 2x faster scalar decomposition",
         "25-50% scalar mult speedup", "medium",
         "name LIKE '%scalar%' AND ct_sensitive=1", ""),
        ("gpu_batch_verify",
         "GPU-parallel batch signature verification",
         "50-200x for large batches", "medium",
         "batchable=1 AND gpu_candidate=1 AND name LIKE '%verify%'", ""),
        ("simd_field_ops",
         "SIMD-parallel field arithmetic using AVX2/NEON",
         "2-4x field throughput", "medium",
         "name LIKE '%field%' AND hot_path=1", ""),
        ("ntt_polynomial",
         "Number Theoretic Transform for polynomial multiplication",
         "n log n vs n^2", "medium",
         "name LIKE '%poly%' AND batchable=1", ""),
        ("precompute_lookup_table",
         "Precomputed lookup tables for fixed-window scalar mult",
         "3-5x vs double-and-add", "low",
         "name LIKE '%ecmult%' AND batchable=0", ""),
    ]
    count = 0
    for pat in patterns:
        try:
            conn.execute(
                "INSERT OR REPLACE INTO optimization_patterns "
                "(pattern_name, description, gain, risk, applicable_when, example_symbol) "
                "VALUES (?,?,?,?,?,?)",
                pat
            )
            count += 1
        except Exception:
            pass
    conn.commit()
    print(f"  Seeded {count} optimization patterns")


def build_analysis_scores(conn):
    """Compute composite analysis scores for every indexed function.

    Scores (all unitless integers):
      hotness_score    — size × name × file-hotspot × recency
      complexity_score — branch + loop + nesting depth
      fanin_score      — weighted call-graph in-degree
      fanout_score     — call-graph out-degree
      gpu_score        — batchability + loop density + name hints
      ct_risk_score    — CT sensitivity + complexity + secret indicators
      audit_gap_score  — hot/CT function with no test coverage
      overall_priority — weighted composite (balanced mode)
    """
    rows = conn.execute(
        "SELECT fi.file_id, fi.qualified_name, fi.name, "
        "fi.start_line, fi.end_line, fi.line_count, "
        "fi.hot_path, fi.ct_sensitive, fi.gpu_candidate, "
        "fi.batchable, fi.recently_modified, f.path "
        "FROM function_index fi JOIN files f ON fi.file_id = f.id "
        "WHERE fi.line_count > 0"
    ).fetchall()

    fan_in_map: dict = {}
    fan_out_map: dict = {}
    for callee, cnt in conn.execute(
        "SELECT callee_func, COUNT(*) FROM call_graph GROUP BY callee_func"
    ):
        fan_in_map[callee] = cnt
    for caller, cnt in conn.execute(
        "SELECT caller_func, COUNT(*) FROM call_graph GROUP BY caller_func"
    ):
        fan_out_map[caller] = cnt

    file_hotspot: dict = dict(conn.execute(
        "SELECT file_id, hotspot_score FROM hotspot_scores"
    ))
    tested_files: set = set(
        r[0] for r in conn.execute(
            "SELECT DISTINCT target_file FROM edges WHERE edge_type='tests'"
        )
    )

    batch_rows = []
    for (file_id, qname, fname, start, end, lc,
         hot, ct, gpu, batch, recently_mod, path) in rows:

        lower_name = fname.lower()
        try:
            src_lines = (ROOT / path).read_text(
                encoding='utf-8', errors='replace'
            ).split('\n')
            body = '\n'.join(src_lines[max(0, start - 1):end])
        except Exception:
            body = ''
        lower_body = body.lower()

        # ── Hotness ──
        hotness = 0
        if lc > 80:   hotness += 3
        elif lc > 40: hotness += 2
        elif lc > 15: hotness += 1
        if hot: hotness += 5
        if any(k in lower_name for k in (
            'mul', 'verify', 'scan', 'hash', 'batch',
            'point', 'field', 'scalar', 'sign',
        )):
            hotness += 3
        if file_hotspot.get(file_id, 0) >= 8: hotness += 2
        if recently_mod:                       hotness += 2

        # ── Complexity ──
        complexity = (
            lower_body.count(' if ') + lower_body.count('\nif ')
            + (lower_body.count('for (') + lower_body.count('for(')) * 2
            + (lower_body.count('while (') + lower_body.count('while(')) * 2
            + lower_body.count('switch (') * 2
        )
        depth = max_depth = 0
        for ch in body:
            if ch == '{':
                depth += 1
                max_depth = max(max_depth, depth)
            elif ch == '}':
                depth = max(0, depth - 1)
        complexity += max(0, max_depth - 2) * 2

        # ── Fan metrics ──
        bare = qname.split('::')[-1]
        fan_in  = fan_in_map.get(bare, 0) + fan_in_map.get(qname, 0)
        fan_out = fan_out_map.get(bare, 0) + fan_out_map.get(qname, 0)
        fan_in_score  = min(fan_in * 2, 15)
        fan_out_score = min(fan_out, 10)

        # ── GPU score ──
        gpu_score = 0
        if batch: gpu_score += 4
        if gpu:   gpu_score += 4
        if any(k in lower_name for k in (
            'batch', 'scan', 'verify', 'ntt', 'fft', 'matmul',
        )):
            gpu_score += 3
        if lower_body.count('for (') + lower_body.count('for(') >= 2:
            gpu_score += 2
        if fan_in >= 3: gpu_score += 2
        if ct:          gpu_score -= 1   # extra care, not a disqualifier

        # ── CT risk ──
        ct_risk = 0
        if ct: ct_risk += 5
        if any(k in lower_name for k in ('secret', 'privkey', 'hmac', 'aes')):
            ct_risk += 3
        if complexity > 10:          ct_risk += 2
        if recently_mod and ct:      ct_risk += 3

        # ── Audit gap ──
        has_tests = file_id in tested_files
        audit_gap = 0
        if not has_tests:              audit_gap += 3
        if hotness >= 5 and not has_tests: audit_gap += 3
        if ct and not has_tests:       audit_gap += 4

        # ── Composite scores ──
        opt_score = hotness * 2 + fan_in_score + gpu_score * 2 - ct_risk
        overall   = (
            hotness * 4
            + fan_in_score * 2
            + gpu_score * 3
            + audit_gap * 3
            - ct_risk * 2
        )

        reasons_parts = []
        if hotness >= 7:    reasons_parts.append(f"hot:{hotness}")
        if fan_in >= 4:     reasons_parts.append(f"fanin:{fan_in}")
        if gpu_score >= 6:  reasons_parts.append(f"gpu:{gpu_score}")
        if complexity >= 8: reasons_parts.append(f"complex:{complexity}")
        if ct_risk >= 5:    reasons_parts.append(f"ct_risk:{ct_risk}")
        if audit_gap >= 5:  reasons_parts.append(f"audit_gap:{audit_gap}")
        if recently_mod:    reasons_parts.append("recent")
        if not reasons_parts: reasons_parts.append("baseline")

        batch_rows.append((
            file_id, qname, path,
            hotness, complexity, fan_in_score, fan_out_score,
            opt_score, gpu_score, ct_risk, audit_gap, overall,
            ','.join(reasons_parts)
        ))

    conn.executemany(
        "INSERT OR REPLACE INTO analysis_scores "
        "(file_id, symbol_name, file_path, "
        "hotness_score, complexity_score, fanin_score, fanout_score, "
        "optimization_score, gpu_score, ct_risk_score, audit_gap_score, "
        "overall_priority, reasons) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
        batch_rows
    )
    conn.commit()

    top5 = conn.execute(
        "SELECT symbol_name, overall_priority, reasons "
        "FROM analysis_scores ORDER BY overall_priority DESC LIMIT 5"
    ).fetchall()
    print(f"  Scored {len(batch_rows)} functions; top 5:")
    for sym, pri, rs in top5:
        short = sym.split('::')[-1] if '::' in sym else sym
        print(f"    {short:<45} priority={pri:>4}  [{(rs or '')[:50]}]")


def build_ai_task_queue(conn):
    """Auto-generate AI reasoning tasks for top-priority bottleneck candidates."""
    conn.execute("DELETE FROM ai_tasks WHERE status='pending'")

    top = conn.execute(
        "SELECT file_id, symbol_name, file_path, "
        "hotness_score, gpu_score, ct_risk_score, audit_gap_score, "
        "overall_priority, reasons "
        "FROM analysis_scores "
        "WHERE overall_priority > 10 "
        "ORDER BY overall_priority DESC LIMIT 30"
    ).fetchall()

    count = 0
    for (fid, sym, fpath, hot, gpu, ct_risk, audit_gap, pri, reasons) in top:
        if ct_risk >= 8:
            task_type = 'ct_review'
            prompt = (
                f"Review `{sym}` for constant-time compliance. "
                f"Check for branches on secrets, variable-time ops, and timing leaks. "
                f"Reasons: {reasons}"
            )
        elif gpu >= 8:
            task_type = 'gpu_candidate'
            prompt = (
                f"Analyse `{sym}` for GPU offload potential. "
                f"Evaluate loop structure, data parallelism, and memory layout. "
                f"Reasons: {reasons}"
            )
        elif audit_gap >= 6:
            task_type = 'audit_expand'
            prompt = (
                f"Write comprehensive tests for `{sym}`. "
                f"Cover edge cases, error paths, and boundary conditions. "
                f"Priority={pri}, file={fpath}"
            )
        else:
            task_type = 'optimize'
            prompt = (
                f"Identify performance improvements for `{sym}`. "
                f"Focus on: algorithmic complexity, SIMD, cache efficiency, "
                f"batch processing. Hotness={hot}, reasons={reasons}"
            )

        conn.execute(
            "INSERT INTO ai_tasks "
            "(file_id, task_type, symbol_name, file_path, "
            "prompt, status, priority, created_at) "
            "VALUES (?,?,?,?,?,'pending',?,datetime('now'))",
            (fid, task_type, sym, fpath, prompt, pri)
        )
        count += 1

    conn.commit()
    print(f"  Generated {count} AI tasks")


def main():
    print(f"Building code graph: {DB_PATH}")
    print(f"Project root: {ROOT}")

    conn = sqlite3.connect(str(DB_PATH))
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")

    print("\n[1/26] Creating schema...")
    create_db(conn)

    print("[2/26] Scanning files...")
    scan_files(conn)

    print("[3/26] Parsing code structure...")
    parse_all(conn)

    print("[4/26] Building implementation map...")
    build_impl_map(conn)

    print("[5/26] Building subsystem graph...")
    build_subsystem_graph(conn)

    print("[6/26] Detecting feature status...")
    detect_features(conn)

    print("[7/26] Building function index...")
    build_function_index(conn)

    print("[8/26] Building file summaries...")
    build_file_summaries(conn)

    print("[9/26] Building unified edges...")
    build_edges(conn)

    print("[10/26] Scanning Qt connections, services, tests...")
    scan_qt_connections(conn)
    scan_services(conn)
    scan_test_cases(conn)

    print("[11/26] Scanning CMake targets...")
    scan_cmake_targets(conn)

    print("[12/26] Building call graph (function-level)...")
    build_call_graph(conn)

    print("[13/26] Scanning XML bindings and validating configs...")
    scan_xml_bindings(conn)

    print("[14/26] Scanning runtime loading entrypoints...")
    scan_runtime_entrypoints(conn)

    print("[15/26] Building symbol alias map (typo detection)...")
    build_symbol_aliases(conn)

    print("[16/26] Building reachability graph...")
    build_reachability(conn)

    print("[17/26] Scoring hotspot files...")
    build_hotspot_scores(conn)

    print("[18/26] Building semantic tags...")
    build_semantic_tags(conn)

    print("[19/26] Building FTS5 search index...")
    build_fts_index(conn)

    print("[20/26] Tracking git delta (recently modified files)...")
    build_git_delta(conn)

    print("[21/26] Detecting duplicate functions...")
    detect_duplicate_functions(conn)

    print("[22/26] Building symbol slices (AI context)...")
    build_symbol_slices(conn)

    print("[23/26] Building function summaries...")
    build_function_summaries(conn)

    print("[24/26] Seeding optimization patterns...")
    build_optimization_patterns(conn)

    print("[25/26] Computing analysis scores (bottleneck ranking)...")
    build_analysis_scores(conn)

    print("[26/26] Generating AI task queue...")
    build_ai_task_queue(conn)

    print_summary(conn)

    conn.close()
    print(f"\nDatabase saved: {DB_PATH}")
    print(f"Size: {DB_PATH.stat().st_size / 1024:.0f} KB")


if __name__ == "__main__":
    main()
