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

import os
import re
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB_PATH = ROOT / "tools" / "codegraph.db"

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
            id              INTEGER PRIMARY KEY,
            file_id         INTEGER REFERENCES files(id),
            name            TEXT NOT NULL,
            qualified_name  TEXT DEFAULT '',
            params          TEXT DEFAULT '',
            return_type     TEXT DEFAULT '',
            start_line      INTEGER NOT NULL,
            end_line        INTEGER NOT NULL,
            line_count      INTEGER DEFAULT 0,
            is_method       INTEGER DEFAULT 0,
            class_name      TEXT DEFAULT ''
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
    """)

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

        for f in funcs:
            conn.execute(
                "INSERT INTO function_index (file_id, name, qualified_name, params, "
                "return_type, start_line, end_line, line_count, is_method, class_name) "
                "VALUES (?,?,?,?,?,?,?,?,?,?)",
                (file_id, f['name'], f.get('qualified_name', f['name']),
                 f.get('params', ''), f.get('return_type', ''),
                 f['start_line'], f['end_line'],
                 f['end_line'] - f['start_line'] + 1,
                 f.get('is_method', 0), f.get('class_name', ''))
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


def main():
    print(f"Building code graph: {DB_PATH}")
    print(f"Project root: {ROOT}")

    if DB_PATH.exists():
        DB_PATH.unlink()

    conn = sqlite3.connect(str(DB_PATH))
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")

    print("\n[1/12] Creating schema...")
    create_db(conn)

    print("[2/12] Scanning files...")
    scan_files(conn)

    print("[3/12] Parsing code structure...")
    parse_all(conn)

    print("[4/12] Building implementation map...")
    build_impl_map(conn)

    print("[5/12] Building subsystem graph...")
    build_subsystem_graph(conn)

    print("[6/12] Detecting feature status...")
    detect_features(conn)

    print("[7/12] Building function index...")
    build_function_index(conn)

    print("[8/12] Building file summaries...")
    build_file_summaries(conn)

    print("[9/12] Building unified edges...")
    build_edges(conn)

    print("[10/12] Scanning Qt connections, services, tests...")
    scan_qt_connections(conn)
    scan_services(conn)
    scan_test_cases(conn)

    print("[11/12] Scanning CMake targets...")
    scan_cmake_targets(conn)

    print("[12/12] Building FTS5 search index...")
    build_fts_index(conn)

    print_summary(conn)

    conn.close()
    print(f"\nDatabase saved: {DB_PATH}")
    print(f"Size: {DB_PATH.stat().st_size / 1024:.0f} KB")


if __name__ == "__main__":
    main()
