#!/usr/bin/env python3
"""
query_graph.py — Interactive query tool for the Exorcist code graph.

Modes:
  python tools/query_graph.py                   # Full overview
  python tools/query_graph.py context <file|class> # Full context: summary+deps+rdeps+tests+functions
  python tools/query_graph.py search <query>      # FTS5 full-text search across all symbols
  python tools/query_graph.py functions <file|cls> # Functions with exact line ranges
  python tools/query_graph.py edges <file>         # All edges for a file
  python tools/query_graph.py summary              # Quick project summary with counts
  python tools/query_graph.py gaps                 # Actionable gap report
  python tools/query_graph.py features             # Feature status table
  python tools/query_graph.py subsystems           # Subsystem stats
  python tools/query_graph.py deps                 # Inter-subsystem dependency edges
  python tools/query_graph.py classes <subsystem>  # List classes in a subsystem
  python tools/query_graph.py interfaces           # All interfaces + implementation status
  python tools/query_graph.py orphans              # Interfaces without implementations
  python tools/query_graph.py ho                   # Header-only classes (non-interface, src/)
  python tools/query_graph.py qobject              # QObject classes without .cpp
  python tools/query_graph.py methods <class>      # Methods of a class
  python tools/query_graph.py services             # ServiceRegistry registrations/resolutions
  python tools/query_graph.py tests                # Test case inventory
  python tools/query_graph.py connections           # Qt signal/slot connections
  python tools/query_graph.py targets              # CMake build targets
  python tools/query_graph.py tags [target]        # Semantic/security/perf/audit tags
  python tools/query_graph.py tagsearch <tag>      # Find entities by semantic tag or value
  python tools/query_graph.py bottlenecks [n]      # Top AI bottleneck candidates (default 20)
  python tools/query_graph.py slice <symbol>       # Symbol slice: signature + critical lines
  python tools/query_graph.py callchain <func>     # Call graph: callers + callees tree
  python tools/query_graph.py aitasks [type]       # AI task queue (all or by type)
  python tools/query_graph.py duplicates           # Duplicate function body groups
  python tools/query_graph.py patterns             # Optimization pattern library
  python tools/query_graph.py sql "<query>"        # Run arbitrary SQL
"""

import sqlite3
import sys
from pathlib import Path

DB = Path(__file__).resolve().parent / 'codegraph.db'
PROJECT_OWNED_FILTER = (
    "f.path LIKE 'src/%' OR f.path LIKE 'plugins/%' OR f.path LIKE 'server/%' "
    "OR f.path LIKE 'tests/%' OR f.path LIKE 'tools/%' OR f.path LIKE 'profiles/%' "
    "OR f.path LIKE 'docs/%'"
)

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _table_exists(conn, name):
    return conn.execute(
        "SELECT COUNT(*) FROM sqlite_master WHERE type IN ('table','view') AND name=?",
        (name,)
    ).fetchone()[0] > 0


def get_conn():
    if not DB.exists():
        print(f"Error: {DB} not found. Run: python tools/build_codegraph.py")
        sys.exit(1)
    return sqlite3.connect(str(DB))


def _semantic_search_clause(raw_tag):
    if ':' in raw_tag:
        tag, value = raw_tag.split(':', 1)
        return "(st.tag = ? AND COALESCE(st.tag_value, '') = ?)", (tag, value)
    return "(st.tag = ? OR COALESCE(st.tag_value, '') = ?)", (raw_tag, raw_tag)


def cmd_features(conn):
    """Show feature status overview."""
    print("══ Features ══")
    tags = {'implemented': '[OK]', 'header-only': '[HO]', 'stub': '[ST]', 'missing': '[--]'}
    for r in conn.execute("SELECT name, status, header_file, source_file FROM features ORDER BY status, name"):
        tag = tags.get(r[1], r[1])
        h = r[2] if r[2] else '-'
        s = r[3] if r[3] else '-'
        print(f"  {tag:5s} {r[0]:<28s} h={h:<45s} s={s}")
    
    counts = {}
    for r in conn.execute("SELECT status, COUNT(*) FROM features GROUP BY status"):
        counts[r[0]] = r[1]
    print(f"\n  Total: {sum(counts.values())}  |  "
          + " | ".join(f"{tags.get(k,k)} {v}" for k, v in sorted(counts.items())))


def cmd_subsystems(conn):
    """Show subsystem statistics."""
    print("══ Subsystems ══")
    print(f"  {'Name':<20s} {'Files':>6s} {'Lines':>8s} {'Classes':>8s} {'Ifaces':>7s}")
    print(f"  {'─'*20} {'─'*6} {'─'*8} {'─'*8} {'─'*7}")
    total_f = total_l = total_c = total_i = 0
    for r in conn.execute("SELECT name, file_count, line_count, class_count, interface_count "
                          "FROM subsystems ORDER BY line_count DESC"):
        print(f"  {r[0]:<20s} {r[1]:>6d} {r[2]:>8d} {r[3]:>8d} {r[4]:>7d}")
        total_f += r[1]; total_l += r[2]; total_c += r[3]; total_i += r[4]
    print(f"  {'─'*20} {'─'*6} {'─'*8} {'─'*8} {'─'*7}")
    print(f"  {'TOTAL':<20s} {total_f:>6d} {total_l:>8d} {total_c:>8d} {total_i:>7d}")


def cmd_deps(conn):
    """Show inter-subsystem dependency edges."""
    print("══ Subsystem Dependencies ══")
    print(f"  {'From':<20s} {'To':<20s} {'Edges':>6s}")
    print(f"  {'─'*20} {'─'*20} {'─'*6}")
    for r in conn.execute("SELECT from_subsystem, to_subsystem, edge_count "
                          "FROM subsystem_deps ORDER BY edge_count DESC"):
        print(f"  {r[0]:<20s} {r[1]:<20s} {r[2]:>6d}")


def cmd_classes(conn, subsystem):
    """List all classes in a subsystem."""
    rows = conn.execute(
        "SELECT c.name, c.is_interface, c.has_impl, f.path "
        "FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE f.subsystem = ? ORDER BY c.name", (subsystem,)
    ).fetchall()
    if not rows:
        print(f"No classes found in subsystem '{subsystem}'")
        return
    print(f"══ Classes in '{subsystem}' ({len(rows)}) ══")
    for r in rows:
        tag = 'I' if r[1] else ('✓' if r[2] else '·')
        print(f"  [{tag}] {r[0]:<40s} {r[3]}")


def cmd_interfaces(conn):
    """Show all interfaces and their implementation status."""
    print("══ Interfaces ══")
    rows = conn.execute(
        "SELECT c.name, c.has_impl, f.path, f.subsystem "
        "FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.is_interface = 1 AND f.path LIKE 'src/%' "
        "ORDER BY f.subsystem, c.name"
    ).fetchall()
    for r in rows:
        tag = '✓' if r[1] else '✗'
        print(f"  [{tag}] {r[0]:<35s} {r[3]:<15s} {r[2]}")
    impl = sum(1 for r in rows if r[1])
    print(f"\n  Total: {len(rows)} interfaces  |  {impl} implemented  |  {len(rows)-impl} orphan")


def cmd_orphans(conn):
    """Show interfaces without implementations."""
    print("══ Orphan Interfaces (no implementation) ══")
    rows = conn.execute(
        "SELECT c.name, f.path, f.subsystem "
        "FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.is_interface = 1 AND c.has_impl = 0 AND f.path LIKE 'src/%' "
        "ORDER BY f.subsystem, c.name"
    ).fetchall()
    for r in rows:
        print(f"  {r[0]:<35s} {r[2]:<15s} {r[1]}")
    print(f"\n  Total: {len(rows)} orphan interfaces")


def cmd_header_only(conn):
    """Show header-only classes in src/ (non-interface)."""
    print("══ Header-Only Classes (src/, non-interface) ══")
    rows = conn.execute(
        "SELECT c.name, f.path, f.has_qobject, f.lines "
        "FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.has_impl = 0 AND c.is_interface = 0 AND c.lang = 'cpp' "
        "AND f.ext = '.h' AND f.path LIKE 'src/%' "
        "ORDER BY f.lines DESC"
    ).fetchall()
    for r in rows:
        qobj = 'Q' if r[2] else ' '
        print(f"  [{qobj}] {r[0]:<40s} {r[3]:>5d}L  {r[1]}")
    qobj_count = sum(1 for r in rows if r[2])
    print(f"\n  Total: {len(rows)} header-only  |  {qobj_count} with QObject")


def cmd_qobject(conn):
    """Show QObject classes without .cpp."""
    print("══ QObject Classes Without .cpp ══")
    rows = conn.execute(
        "SELECT c.name, f.path, f.lines "
        "FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.has_impl = 0 AND c.is_interface = 0 "
        "AND f.has_qobject = 1 AND f.ext = '.h' AND f.path LIKE 'src/%' "
        "ORDER BY f.subsystem, c.name"
    ).fetchall()
    for r in rows:
        print(f"  {r[0]:<40s} {r[2]:>5d}L  {r[1]}")
    print(f"\n  Total: {len(rows)} QObject classes needing .cpp files")


def cmd_methods(conn, classname):
    """Show methods of a class."""
    rows = conn.execute(
        "SELECT m.name, m.is_pure_virtual, m.is_signal, m.is_slot, m.has_body "
        "FROM methods m JOIN classes c ON m.class_id = c.id "
        "WHERE c.name = ? ORDER BY m.name", (classname,)
    ).fetchall()
    if not rows:
        print(f"No methods found for class '{classname}'")
        return
    print(f"══ Methods of '{classname}' ({len(rows)}) ══")
    for r in rows:
        flags = []
        if r[1]: flags.append('virtual')
        if r[2]: flags.append('signal')
        if r[3]: flags.append('slot')
        if r[4]: flags.append('body')
        flag_str = ', '.join(flags) if flags else ''
        print(f"  {r[0]:<45s} {flag_str}")


def cmd_sql(conn, query):
    """Run arbitrary SQL."""
    try:
        rows = conn.execute(query).fetchall()
        if not rows:
            print("(no results)")
            return
        # Get column names
        desc = conn.execute(query).description
        cols = [d[0] for d in desc]
        print('  '.join(f'{c:<30s}' for c in cols))
        print('  '.join('─' * 30 for _ in cols))
        for r in rows:
            print('  '.join(f'{str(v):<30s}' for v in r))
        print(f"\n({len(rows)} rows)")
    except Exception as e:
        print(f"SQL error: {e}")


def cmd_tags(conn, target=None):
    if not _table_exists(conn, 'semantic_tags'):
        print("Semantic tags not available. Rebuild with: python tools/build_codegraph.py")
        return
    if target:
        is_file_like = '.' in target or '/' in target or '\\' in target
        if is_file_like:
            row = conn.execute(
                "SELECT id, path FROM files WHERE path = ? OR path LIKE ?",
                (target, f'%{target}%')
            ).fetchone()
        else:
            row = conn.execute(
                "SELECT f.id, f.path FROM classes c JOIN files f ON c.file_id = f.id WHERE c.name = ?",
                (target,)
            ).fetchone()
            if not row:
                row = conn.execute(
                    "SELECT id, path FROM files WHERE path LIKE ?",
                    (f'%{target}%',)
                ).fetchone()
        if not row:
            print(f"Not found: '{target}'")
            return
        rows = conn.execute(
            "SELECT entity_type, entity_name, line_num, tag, tag_value, confidence "
            "FROM semantic_tags WHERE file_id = ? "
            "ORDER BY entity_type, tag, tag_value, entity_name",
            (row[0],)
        ).fetchall()
        print(f"══ Semantic Tags: {row[1]} ══")
    else:
        rows = conn.execute(
            "SELECT f.path, entity_type, entity_name, line_num, tag, tag_value, confidence "
            "FROM semantic_tags st JOIN files f ON st.file_id = f.id "
            "WHERE f.path NOT LIKE 'ReserchRepos/%' "
            f"ORDER BY CASE WHEN {PROJECT_OWNED_FILTER} THEN 0 ELSE 1 END, "
            "f.path, entity_type, tag, tag_value LIMIT 300"
        ).fetchall()
        print("══ Semantic Tags (first 300) ══")
    for r in rows:
        if target:
            print(f"  [{r[0]:<8}] {r[3]}:{r[4] or '-':<22} conf={r[5]:>3}  {r[1]} @L{r[2]}")
        else:
            print(f"  {r[0]} :: [{r[1]}] {r[4]}:{r[5] or '-'}  {r[2]} @L{r[3]} conf={r[6]}")


def cmd_tagsearch(conn, tag):
    if not _table_exists(conn, 'semantic_tags'):
        print("Semantic tags not available. Rebuild with: python tools/build_codegraph.py")
        return
    where_clause, params = _semantic_search_clause(tag)
    rows = conn.execute(
        "SELECT f.path, entity_type, entity_name, line_num, tag, tag_value, confidence "
        "FROM semantic_tags st JOIN files f ON st.file_id = f.id "
        f"WHERE {where_clause} "
        "AND f.path NOT LIKE 'ReserchRepos/%' "
        f"ORDER BY CASE WHEN {PROJECT_OWNED_FILTER} THEN 0 ELSE 1 END, "
        "confidence DESC, f.path LIMIT 100",
        params
    ).fetchall()
    print(f"══ Tag Search: {tag} ({len(rows)}) ══")
    for r in rows:
        print(f"  {r[0]} :: [{r[1]}] {r[4]}:{r[5] or '-'}  {r[2]} @L{r[3]} conf={r[6]}")


def cmd_context(conn, target):
    """Full context for a file or class: summary + deps + rdeps + tests + functions."""
    is_file_like = '.' in target or '/' in target or '\\' in target

    if is_file_like:
        file_row = conn.execute(
            "SELECT id, path, lang, lines, subsystem FROM files "
            "WHERE path = ? OR path LIKE ?",
            (target, f'%{target}%')
        ).fetchone()
    else:
        # Try class name first for bare names
        cls_row = conn.execute(
            "SELECT c.name, f.id, f.path, f.lang, f.lines, f.subsystem "
            "FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE c.name = ?", (target,)
        ).fetchone()
        if cls_row:
            file_row = (cls_row[1], cls_row[2], cls_row[3], cls_row[4], cls_row[5])
        else:
            file_row = conn.execute(
                "SELECT id, path, lang, lines, subsystem FROM files "
                "WHERE path LIKE ? AND ext IN ('.cpp', '.h', '.py')",
                (f'%{target}%',)
            ).fetchone()

    if not file_row:
        # Try as class name
        cls_row = conn.execute(
            "SELECT c.name, f.id, f.path, f.lang, f.lines, f.subsystem "
            "FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE c.name = ?", (target,)
        ).fetchone()
        if cls_row:
            file_row = (cls_row[1], cls_row[2], cls_row[3], cls_row[4], cls_row[5])
        else:
            print(f"Not found: '{target}'")
            return

    file_id, path, lang, line_count, subsystem = file_row

    print(f"\n══ CONTEXT: {path} ══\n")

    # Summary
    if _table_exists(conn, 'file_summaries'):
        summ = conn.execute(
            "SELECT summary, category, key_classes FROM file_summaries WHERE file_id = ?",
            (file_id,)
        ).fetchone()
        if summ:
            print(f"  Summary:    {summ[0]}")
            print(f"  Category:   {summ[1]}")
            if summ[2]:
                print(f"  Classes:    {summ[2]}")
    print(f"  Lines:      {line_count}")
    print(f"  Subsystem:  {subsystem}")

    # Dependencies (this file includes)
    if _table_exists(conn, 'edges'):
        deps = conn.execute(
            "SELECT DISTINCT f.path FROM edges e "
            "JOIN files f ON e.target_file = f.id "
            "WHERE e.source_file = ? AND e.edge_type = 'includes'",
            (file_id,)
        ).fetchall()
        if deps:
            print(f"\n  ── Dependencies ({len(deps)}) ──")
            for (d,) in deps:
                print(f"    → {d}")

        # Reverse dependencies
        rdeps = conn.execute(
            "SELECT DISTINCT f.path FROM edges e "
            "JOIN files f ON e.source_file = f.id "
            "WHERE e.target_file = ? AND e.edge_type = 'includes'",
            (file_id,)
        ).fetchall()
        if rdeps:
            print(f"\n  ── Reverse Dependencies ({len(rdeps)}) ──")
            for (r,) in rdeps:
                print(f"    ← {r}")

        # Tests
        test_edges = conn.execute(
            "SELECT DISTINCT f.path FROM edges e "
            "JOIN files f ON e.source_file = f.id "
            "WHERE e.target_file = ? AND e.edge_type = 'tests'",
            (file_id,)
        ).fetchall()
        if test_edges:
            print(f"\n  ── Tests ──")
            for (tf,) in test_edges:
                print(f"    {tf}")
                if _table_exists(conn, 'test_cases'):
                    tf_id = conn.execute(
                        "SELECT id FROM files WHERE path = ?", (tf,)
                    ).fetchone()
                    if tf_id:
                        methods = conn.execute(
                            "SELECT test_method, line_num FROM test_cases "
                            "WHERE file_id = ? ORDER BY line_num",
                            (tf_id[0],)
                        ).fetchall()
                        for tm, ln in methods:
                            print(f"      • {tm} (L{ln})")

    # Functions with line ranges
    if _table_exists(conn, 'function_index'):
        funcs = conn.execute(
            "SELECT qualified_name, start_line, end_line, line_count "
            "FROM function_index WHERE file_id = ? ORDER BY start_line",
            (file_id,)
        ).fetchall()
        if funcs:
            print(f"\n  ── Functions ({len(funcs)}) ──")
            for f in funcs:
                print(f"    {f[0]:<50s} L{f[1]}-L{f[2]}  ({f[3]} lines)")

    # Qt connections in this file
    if _table_exists(conn, 'qt_connections'):
        conns = conn.execute(
            "SELECT sender, signal_name, receiver, slot_name, line_num "
            "FROM qt_connections WHERE file_id = ?",
            (file_id,)
        ).fetchall()
        if conns:
            print(f"\n  ── Qt Connections ({len(conns)}) ──")
            for c in conns:
                print(f"    {c[0]}.{c[1]} → {c[2]}.{c[3]}  (L{c[4]})")

    # Services used/provided
    if _table_exists(conn, 'services'):
        svcs = conn.execute(
            "SELECT service_key, reg_type, line_num FROM services WHERE file_id = ?",
            (file_id,)
        ).fetchall()
        if svcs:
            print(f"\n  ── Services ──")
            for s in svcs:
                icon = '>>' if s[1] == 'register' else '<<'
                print(f"    {icon} {s[0]} ({s[1]}, L{s[2]})")

    if _table_exists(conn, 'semantic_tags'):
        tags = conn.execute(
            "SELECT entity_type, entity_name, line_num, tag, tag_value, confidence "
            "FROM semantic_tags WHERE file_id = ? "
            "ORDER BY entity_type, tag, tag_value, entity_name",
            (file_id,)
        ).fetchall()
        if tags:
            print(f"\n  ── Semantic Tags ({len(tags)}) ──")
            for t in tags[:80]:
                print(f"    [{t[0]:<8}] {t[3]}:{t[4] or '-':<22} conf={t[5]:>3}  {t[1]} @L{t[2]}")


def cmd_search(conn, query):
    """Full-text search across all indexed symbols."""
    if not _table_exists(conn, 'fts_index'):
        print("FTS5 index not available. Rebuild with: python tools/build_codegraph.py")
        return

    try:
        results = conn.execute(
            "SELECT name, qualified_name, file_path, kind, summary, rank "
            "FROM fts_index WHERE fts_index MATCH ? ORDER BY rank LIMIT 30",
            (query,)
        ).fetchall()
    except Exception:
        results = conn.execute(
            "SELECT name, qualified_name, file_path, kind, summary, rank "
            "FROM fts_index WHERE fts_index MATCH ? ORDER BY rank LIMIT 30",
            (f'"{query}"',)
        ).fetchall()

    if not results:
        print(f"No results for '{query}'")
        return

    print(f"══ Search: '{query}' ({len(results)} results) ══")
    icons = {'file': 'F', 'class': 'C', 'function': 'f', 'service': 'S', 'test': 'T'}
    for r in results:
        icon = icons.get(r[3], '.')
        summ = f" -- {r[4]}" if r[4] else ''
        print(f"  [{icon}] [{r[3]:8s}] {r[1]:<45s} {r[2]}{summ}")


def cmd_functions(conn, target):
    """Show functions in a file or class with line ranges."""
    if not _table_exists(conn, 'function_index'):
        print("Function index not available. Rebuild with: python tools/build_codegraph.py")
        return

    # If target looks like a file (has extension or path sep), search files first
    is_file_like = '.' in target or '/' in target or '\\' in target

    if is_file_like:
        file_row = conn.execute(
            "SELECT id, path FROM files WHERE path = ? OR path LIKE ?",
            (target, f'%{target}%')
        ).fetchone()
    else:
        file_row = None

    if file_row:
        file_id, path = file_row
        funcs = conn.execute(
            "SELECT qualified_name, start_line, end_line, line_count, class_name "
            "FROM function_index WHERE file_id = ? ORDER BY start_line",
            (file_id,)
        ).fetchall()
        if not funcs:
            print(f"No functions found in {path}")
            return
        print(f"══ Functions in {path} ({len(funcs)}) ══")
        for f in funcs:
            print(f"  {f[0]:<55s} L{f[1]:<5d}-L{f[2]:<5d} ({f[3]} lines)")
    else:
        # Try as class name first
        funcs = conn.execute(
            "SELECT fi.qualified_name, fi.start_line, fi.end_line, fi.line_count, f.path "
            "FROM function_index fi JOIN files f ON fi.file_id = f.id "
            "WHERE fi.class_name = ? ORDER BY f.path, fi.start_line",
            (target,)
        ).fetchall()
        if not funcs:
            # Fallback: try fuzzy file path match
            file_row = conn.execute(
                "SELECT id, path FROM files WHERE path LIKE ? AND ext = '.cpp'",
                (f'%{target}%',)
            ).fetchone()
            if file_row:
                funcs_f = conn.execute(
                    "SELECT qualified_name, start_line, end_line, line_count, class_name "
                    "FROM function_index WHERE file_id = ? ORDER BY start_line",
                    (file_row[0],)
                ).fetchall()
                if funcs_f:
                    print(f"══ Functions in {file_row[1]} ({len(funcs_f)}) ══")
                    for f in funcs_f:
                        print(f"  {f[0]:<55s} L{f[1]:<5d}-L{f[2]:<5d} ({f[3]} lines)")
                    return
            print(f"No functions found for '{target}'")
            return
        print(f"══ Functions of {target} ({len(funcs)}) ══")
        for f in funcs:
            print(f"  {f[0]:<55s} L{f[1]:<5d}-L{f[2]:<5d} ({f[3]} lines) [{f[4]}]")


def cmd_edges_query(conn, target):
    """Show all edges for a file."""
    if not _table_exists(conn, 'edges'):
        print("Edges table not available. Rebuild with: python tools/build_codegraph.py")
        return

    is_file_like = '.' in target or '/' in target or '\\' in target
    if is_file_like:
        file_row = conn.execute(
            "SELECT id, path FROM files WHERE path = ? OR path LIKE ?",
            (target, f'%{target}%')
        ).fetchone()
    else:
        # Try class name first
        cls = conn.execute(
            "SELECT f.id, f.path FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE c.name = ?", (target,)
        ).fetchone()
        file_row = cls if cls else conn.execute(
            "SELECT id, path FROM files WHERE path LIKE ? AND ext IN ('.cpp', '.h', '.py')",
            (f'%{target}%',)
        ).fetchone()
    if not file_row:
        print(f"Not found: '{target}'")
        return

    file_id, path = file_row

    outgoing = conn.execute(
        "SELECT e.edge_type, e.target_name, f.path "
        "FROM edges e LEFT JOIN files f ON e.target_file = f.id "
        "WHERE e.source_file = ? ORDER BY e.edge_type",
        (file_id,)
    ).fetchall()

    incoming = conn.execute(
        "SELECT e.edge_type, e.source_name, f.path "
        "FROM edges e LEFT JOIN files f ON e.source_file = f.id "
        "WHERE e.target_file = ? ORDER BY e.edge_type",
        (file_id,)
    ).fetchall()

    print(f"\n══ Edges for {path} ══")
    if outgoing:
        print(f"\n  Outgoing ({len(outgoing)}):")
        for e in outgoing:
            print(f"    → [{e[0]}] {e[2] or e[1]}")
    if incoming:
        print(f"\n  Incoming ({len(incoming)}):")
        for e in incoming:
            print(f"    ← [{e[0]}] {e[2] or e[1]}")
    if not outgoing and not incoming:
        print("  (no edges)")


def cmd_project_summary(conn):
    """Quick project summary with counts."""
    files = conn.execute("SELECT COUNT(*) FROM files").fetchone()[0]
    classes = conn.execute("SELECT COUNT(*) FROM classes").fetchone()[0]

    print(f"══ Project Summary ══")
    print(f"  Files:          {files}")
    print(f"  Classes:        {classes}")

    if _table_exists(conn, 'function_index'):
        funcs = conn.execute("SELECT COUNT(*) FROM function_index").fetchone()[0]
        print(f"  Functions:      {funcs}")
    if _table_exists(conn, 'edges'):
        edges_n = conn.execute("SELECT COUNT(*) FROM edges").fetchone()[0]
        print(f"  Edges:          {edges_n}")
    if _table_exists(conn, 'test_cases'):
        tests_n = conn.execute("SELECT COUNT(*) FROM test_cases").fetchone()[0]
        print(f"  Tests:          {tests_n}")
    if _table_exists(conn, 'services'):
        svcs = conn.execute("SELECT COUNT(*) FROM services").fetchone()[0]
        print(f"  Services:       {svcs}")
    if _table_exists(conn, 'qt_connections'):
        conns_n = conn.execute("SELECT COUNT(*) FROM qt_connections").fetchone()[0]
        print(f"  Qt Connections: {conns_n}")
    if _table_exists(conn, 'cmake_targets'):
        targets = conn.execute("SELECT COUNT(*) FROM cmake_targets").fetchone()[0]
        print(f"  CMake Targets:  {targets}")
    if _table_exists(conn, 'semantic_tags'):
        tag_count = conn.execute("SELECT COUNT(*) FROM semantic_tags").fetchone()[0]
        print(f"  Semantic Tags:  {tag_count}")

    if _table_exists(conn, 'edges'):
        print(f"\n  Edge Types:")
        for row in conn.execute(
            "SELECT edge_type, COUNT(*) FROM edges GROUP BY edge_type "
            "ORDER BY COUNT(*) DESC"
        ):
            print(f"    {row[0]:<15s} {row[1]}")

    if _table_exists(conn, 'file_summaries'):
        print(f"\n  File Categories:")
        for row in conn.execute(
            "SELECT category, COUNT(*) FROM file_summaries GROUP BY category "
            "ORDER BY COUNT(*) DESC"
        ):
            print(f"    {row[0]:<15s} {row[1]}")


def cmd_services_query(conn):
    """Show ServiceRegistry registrations/resolutions."""
    if not _table_exists(conn, 'services'):
        print("Services table not available. Rebuild with: python tools/build_codegraph.py")
        return

    rows = conn.execute(
        "SELECT s.service_key, s.reg_type, s.line_num, f.path "
        "FROM services s JOIN files f ON s.file_id = f.id "
        "ORDER BY s.service_key, s.reg_type"
    ).fetchall()
    if not rows:
        print("No service registrations found.")
        return

    print(f"══ Services ({len(rows)}) ══")
    for r in rows:
        icon = '>>' if r[1] == 'register' else '<<'
        print(f"  {icon} {r[0]:<30s} {r[1]:<10s} L{r[2]:<5d} {r[3]}")


def cmd_tests_query(conn):
    """Show test case inventory."""
    if not _table_exists(conn, 'test_cases'):
        print("Test cases table not available. Rebuild with: python tools/build_codegraph.py")
        return

    rows = conn.execute(
        "SELECT tc.test_class, tc.test_method, tc.line_num, f.path "
        "FROM test_cases tc JOIN files f ON tc.file_id = f.id "
        "ORDER BY tc.test_class, tc.line_num"
    ).fetchall()
    if not rows:
        print("No test cases found.")
        return

    print(f"══ Test Cases ({len(rows)}) ══")
    current_class = ''
    for r in rows:
        if r[0] != current_class:
            current_class = r[0]
            print(f"\n  {current_class} ({r[3]})")
        print(f"    {r[1]:<40s} L{r[2]}")


def cmd_connections_query(conn):
    """Show Qt signal/slot connections."""
    if not _table_exists(conn, 'qt_connections'):
        print("Qt connections not available. Rebuild with: python tools/build_codegraph.py")
        return

    rows = conn.execute(
        "SELECT q.sender, q.signal_name, q.receiver, q.slot_name, q.line_num, f.path "
        "FROM qt_connections q JOIN files f ON q.file_id = f.id "
        "ORDER BY f.path, q.line_num"
    ).fetchall()
    if not rows:
        print("No Qt connections found.")
        return

    print(f"══ Qt Connections ({len(rows)}) ══")
    for r in rows:
        print(f"  {r[0]}.{r[1]} → {r[2]}.{r[3]}  L{r[4]}  {r[5]}")


def cmd_targets_query(conn):
    """Show CMake build targets."""
    if not _table_exists(conn, 'cmake_targets'):
        print("CMake targets not available. Rebuild with: python tools/build_codegraph.py")
        return

    rows = conn.execute(
        "SELECT ct.target_name, ct.target_type, ct.line_num, "
        "COALESCE(f.path, '(CMakeLists.txt)') "
        "FROM cmake_targets ct LEFT JOIN files f ON ct.file_id = f.id "
        "ORDER BY ct.target_type, ct.target_name"
    ).fetchall()
    if not rows:
        print("No CMake targets found.")
        return

    print(f"══ CMake Targets ({len(rows)}) ══")
    for r in rows:
        print(f"  [{r[1]:10s}] {r[0]:<30s} L{r[2]:<5d} {r[3]}")


def cmd_bottlenecks(conn, n=20):
    """Show top AI bottleneck candidates from analysis_scores."""
    if not _table_exists(conn, 'analysis_scores'):
        print("analysis_scores not available. Rebuild with: python tools/build_codegraph.py")
        return
    try:
        limit = int(n)
    except (TypeError, ValueError):
        limit = 20

    rows = conn.execute(
        "SELECT symbol_name, file_path, hotness_score, complexity_score, "
        "fanin_score, gpu_score, ct_risk_score, audit_gap_score, "
        "overall_priority, reasons "
        "FROM analysis_scores ORDER BY overall_priority DESC LIMIT ?",
        (limit,)
    ).fetchall()
    if not rows:
        print("No analysis scores found — rebuild the graph first.")
        return

    print(f"\u2550\u2550 Top {limit} Bottleneck Candidates \u2550\u2550")
    print(f"  {'Symbol':<40} {'Pri':>4} {'Hot':>4} {'GPU':>4} {'CT':>4} {'Gap':>4}  Reasons")
    print(f"  {'\u2500'*40} {'\u2500'*4} {'\u2500'*4} {'\u2500'*4} {'\u2500'*4} {'\u2500'*4}  {'\u2500'*40}")
    for r in rows:
        sym   = r[0].split('::')[-1] if '::' in r[0] else r[0]
        print(f"  {sym:<40} {r[8]:>4} {r[2]:>4} {r[5]:>4} {r[6]:>4} {r[7]:>4}  {(r[9] or '')[:40]}")
    print(f"\n  Use: python tools/query_graph.py slice <symbol>  to get the code slice")


def cmd_slice(conn, symbol):
    """Show symbol slice: signature + critical lines (low-token context)."""
    if not _table_exists(conn, 'symbol_slices'):
        print("symbol_slices not available. Rebuild with: python tools/build_codegraph.py")
        return

    row = conn.execute(
        "SELECT ss.symbol, ss.signature, ss.critical_lines, "
        "ss.slice_token_estimate, ss.full_token_estimate, f.path "
        "FROM symbol_slices ss JOIN files f ON ss.file_id = f.id "
        "WHERE ss.symbol LIKE ? OR ss.symbol LIKE ? "
        "ORDER BY length(ss.symbol) LIMIT 1",
        (symbol, f'%::{symbol}')
    ).fetchone()
    if not row:
        print(f"No slice found for '{symbol}'")
        return

    sym, sig, critical, slice_tok, full_tok, path = row
    savings = round((1 - slice_tok / max(full_tok, 1)) * 100)
    print(f"\u2550\u2550 Slice: {sym} \u2550\u2550")
    print(f"  File:            {path}")
    print(f"  Token estimate:  slice={slice_tok}  full={full_tok}  savings={savings}%")
    print(f"\n  Signature:")
    print(f"    {sig}")
    if critical:
        print(f"\n  Critical lines:")
        for ln in critical.split('\n'):
            print(f"    {ln}")

    # Also show metadata flags if available
    if _table_exists(conn, 'function_summary'):
        meta = conn.execute(
            "SELECT category, batchable, gpu_candidate, ct_sensitive, recently_modified "
            "FROM function_summary WHERE qualified_symbol LIKE ? OR symbol LIKE ? LIMIT 1",
            (f'%{symbol}%', f'%{symbol}%')
        ).fetchone()
        if meta:
            flags = []
            if meta[1]: flags.append('batchable')
            if meta[2]: flags.append('gpu_candidate')
            if meta[3]: flags.append('ct_sensitive')
            if meta[4]: flags.append('recently_modified')
            print(f"\n  Category: {meta[0]}  Flags: {', '.join(flags) or 'none'}")


def cmd_callchain(conn, func):
    """Show call graph: callers and callees for a function."""
    if not _table_exists(conn, 'call_graph'):
        print("call_graph not available. Rebuild with: python tools/build_codegraph.py")
        return

    callees = conn.execute(
        "SELECT DISTINCT cg.callee_func, f.path "
        "FROM call_graph cg LEFT JOIN files f ON cg.callee_file = f.id "
        "WHERE cg.caller_func LIKE ? OR cg.caller_func LIKE ? "
        "ORDER BY cg.callee_func",
        (func, f'%::{func}')
    ).fetchall()

    callers = conn.execute(
        "SELECT DISTINCT cg.caller_func, f.path "
        "FROM call_graph cg LEFT JOIN files f ON cg.caller_file = f.id "
        "WHERE cg.callee_func = ? OR cg.callee_func LIKE ? "
        "ORDER BY cg.caller_func",
        (func, f'%::{func}')
    ).fetchall()

    print(f"\u2550\u2550 Call Chain: {func} \u2550\u2550")
    if callers:
        print(f"\n  Called by ({len(callers)}):")
        for f_name, path in callers:
            print(f"    \u2190 {f_name:<45}  {path or '?'}")
    else:
        print(f"\n  Called by: (none found — may be an entrypoint)")

    if callees:
        print(f"\n  Calls ({len(callees)}):")
        for f_name, path in callees:
            print(f"    \u2192 {f_name:<45}  {path or '?'}")
    else:
        print(f"\n  Calls: (none found)")

    # Show analysis score if available
    if _table_exists(conn, 'analysis_scores'):
        score = conn.execute(
            "SELECT overall_priority, hotness_score, gpu_score, ct_risk_score, reasons "
            "FROM analysis_scores WHERE symbol_name LIKE ? OR symbol_name LIKE ? LIMIT 1",
            (func, f'%::{func}')
        ).fetchone()
        if score:
            print(f"\n  Analysis: priority={score[0]}  hot={score[1]}  "
                  f"gpu={score[2]}  ct_risk={score[3]}  [{score[4]}]")


def cmd_aitasks(conn, task_type=None):
    """Show AI task queue."""
    if not _table_exists(conn, 'ai_tasks'):
        print("ai_tasks not available. Rebuild with: python tools/build_codegraph.py")
        return

    if task_type:
        rows = conn.execute(
            "SELECT task_type, symbol_name, file_path, priority, status, prompt "
            "FROM ai_tasks WHERE task_type = ? ORDER BY priority DESC",
            (task_type,)
        ).fetchall()
    else:
        rows = conn.execute(
            "SELECT task_type, symbol_name, file_path, priority, status, prompt "
            "FROM ai_tasks ORDER BY priority DESC"
        ).fetchall()

    if not rows:
        print("No AI tasks found — rebuild the graph first.")
        return

    print(f"\u2550\u2550 AI Task Queue ({len(rows)}) \u2550\u2550")
    _type_icons = {
        'optimize': '[OPT]', 'gpu_candidate': '[GPU]',
        'ct_review': '[CT!]', 'audit_expand': '[TST]',
    }
    for r in rows:
        icon = _type_icons.get(r[0], '[???]')
        sym = r[1].split('::')[-1] if '::' in r[1] else r[1]
        print(f"  {icon} {sym:<40} pri={r[3]:>4}  [{r[4]}]")
        print(f"       {r[5][:100]}")
        print()

    types = conn.execute(
        "SELECT task_type, COUNT(*) FROM ai_tasks GROUP BY task_type"
    ).fetchall()
    print(f"  Types: " + '  '.join(f"{t}: {c}" for t, c in types))


def cmd_duplicates(conn):
    """Show duplicate function body groups."""
    if not _table_exists(conn, 'function_index'):
        print("function_index not available. Rebuild with: python tools/build_codegraph.py")
        return

    groups = conn.execute(
        "SELECT DISTINCT duplicate_group FROM function_index "
        "WHERE duplicate_group != '' ORDER BY duplicate_group"
    ).fetchall()
    if not groups:
        print("No duplicate function bodies detected.")
        return

    print(f"\u2550\u2550 Duplicate Function Groups ({len(groups)}) \u2550\u2550")
    for (group,) in groups:
        members = conn.execute(
            "SELECT fi.qualified_name, f.path, fi.line_count "
            "FROM function_index fi JOIN files f ON fi.file_id = f.id "
            "WHERE fi.duplicate_group = ? ORDER BY f.path",
            (group,)
        ).fetchall()
        print(f"\n  Group: {group}")
        for qn, path, lc in members:
            print(f"    {qn:<50}  {lc:>4}L  {path}")


def cmd_patterns(conn):
    """Show optimization pattern library."""
    if not _table_exists(conn, 'optimization_patterns'):
        print("optimization_patterns not available. Rebuild with: python tools/build_codegraph.py")
        return

    rows = conn.execute(
        "SELECT pattern_name, gain, risk, description, applicable_when, example_symbol "
        "FROM optimization_patterns ORDER BY gain DESC, pattern_name"
    ).fetchall()
    if not rows:
        print("No optimization patterns found.")
        return

    print(f"\u2550\u2550 Optimization Patterns ({len(rows)}) \u2550\u2550")
    for name, gain, risk, desc, when, ex in rows:
        print(f"\n  [{risk.upper():6s}] {name}")
        print(f"    Gain : {gain}")
        print(f"    Desc : {desc}")
        print(f"    When : {when}")
        if ex:
            print(f"    Ex   : {ex}")


def cmd_overview(conn):
    """Full overview."""
    cmd_features(conn)
    print()
    cmd_subsystems(conn)
    print()
    # Quick summary of gaps
    ho = conn.execute("SELECT COUNT(*) FROM features WHERE status='header-only'").fetchone()[0]
    st = conn.execute("SELECT COUNT(*) FROM features WHERE status='stub'").fetchone()[0]
    mi = conn.execute("SELECT COUNT(*) FROM features WHERE status='missing'").fetchone()[0]
    qobj = conn.execute(
        "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.has_impl = 0 AND c.is_interface = 0 AND f.has_qobject = 1 "
        "AND f.ext = '.h' AND f.path LIKE 'src/%'"
    ).fetchone()[0]
    orphan = conn.execute(
        "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
        "WHERE c.is_interface = 1 AND c.has_impl = 0 AND f.path LIKE 'src/%'"
    ).fetchone()[0]
    print(f"\n── Quick Gaps ──")
    print(f"  Features: {ho} header-only, {st} stub, {mi} missing")
    print(f"  QObject without .cpp: {qobj}")
    print(f"  Orphan interfaces: {orphan}")


def main():
    conn = get_conn()
    args = sys.argv[1:]

    if not args:
        cmd_overview(conn)
    elif args[0] == 'context' and len(args) > 1:
        cmd_context(conn, ' '.join(args[1:]))
    elif args[0] == 'search' and len(args) > 1:
        cmd_search(conn, ' '.join(args[1:]))
    elif args[0] == 'functions' and len(args) > 1:
        cmd_functions(conn, args[1])
    elif args[0] == 'edges' and len(args) > 1:
        cmd_edges_query(conn, args[1])
    elif args[0] == 'summary':
        cmd_project_summary(conn)
    elif args[0] == 'gaps':
        cmd_overview(conn)  # includes gap report
    elif args[0] == 'features':
        cmd_features(conn)
    elif args[0] == 'subsystems':
        cmd_subsystems(conn)
    elif args[0] == 'deps':
        cmd_deps(conn)
    elif args[0] == 'classes' and len(args) > 1:
        cmd_classes(conn, args[1])
    elif args[0] == 'interfaces':
        cmd_interfaces(conn)
    elif args[0] == 'orphans':
        cmd_orphans(conn)
    elif args[0] == 'ho':
        cmd_header_only(conn)
    elif args[0] == 'qobject':
        cmd_qobject(conn)
    elif args[0] == 'methods' and len(args) > 1:
        cmd_methods(conn, args[1])
    elif args[0] == 'services':
        cmd_services_query(conn)
    elif args[0] == 'tests':
        cmd_tests_query(conn)
    elif args[0] == 'connections':
        cmd_connections_query(conn)
    elif args[0] == 'targets':
        cmd_targets_query(conn)
    elif args[0] == 'tags':
        cmd_tags(conn, ' '.join(args[1:]) if len(args) > 1 else None)
    elif args[0] == 'tagsearch' and len(args) > 1:
        cmd_tagsearch(conn, ' '.join(args[1:]))
    elif args[0] == 'bottlenecks':
        cmd_bottlenecks(conn, args[1] if len(args) > 1 else 20)
    elif args[0] == 'slice' and len(args) > 1:
        cmd_slice(conn, args[1])
    elif args[0] == 'callchain' and len(args) > 1:
        cmd_callchain(conn, args[1])
    elif args[0] == 'aitasks':
        cmd_aitasks(conn, args[1] if len(args) > 1 else None)
    elif args[0] == 'duplicates':
        cmd_duplicates(conn)
    elif args[0] == 'patterns':
        cmd_patterns(conn)
    elif args[0] == 'sql' and len(args) > 1:
        cmd_sql(conn, ' '.join(args[1:]))
    else:
        print(__doc__)

    conn.close()


if __name__ == '__main__':
    main()
