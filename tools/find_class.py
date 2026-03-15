#!/usr/bin/env python3
"""
Exorcist IDE — Code Graph Quick Lookup
Find classes, files, methods, and their relationships instantly.

Usage:
    python tools/find_class.py SearchWorker          # Find class by name
    python tools/find_class.py --file editorview     # Find files by name
    python tools/find_class.py --methods EditorView  # List methods of a class
    python tools/find_class.py --impl IFileSystem    # Find implementors of interface
    python tools/find_class.py --deps editorview.h   # What includes this file?
    python tools/find_class.py --sub agent           # List classes in subsystem
    python tools/find_class.py --who-uses AgentSession  # Who inherits/references this?
"""
import sqlite3
import sys
import argparse
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent / "codegraph.db"


def get_conn():
    if not DB_PATH.exists():
        print("ERROR: codegraph.db not found. Run build_codegraph.py first.")
        sys.exit(1)
    return sqlite3.connect(str(DB_PATH))


def find_class(name):
    conn = get_conn()
    rows = conn.execute("""
        SELECT c.name, c.bases, c.is_interface, c.has_impl, c.line_num,
               f.path, f.subsystem, f.lines
        FROM classes c JOIN files f ON c.file_id = f.id
        WHERE c.name LIKE ? AND c.lang = 'cpp'
        ORDER BY f.path
    """, (f"%{name}%",)).fetchall()

    if not rows:
        print(f"  No classes matching '{name}'")
        return

    print(f"  Classes matching '{name}' ({len(rows)}):")
    print(f"  {'Class':<30} {'Bases':<25} {'IF':>3} {'Impl':>5} {'Line':>5} {'Subsystem':<12} Path")
    print(f"  {'-'*30} {'-'*25} {'-'*3} {'-'*5} {'-'*5} {'-'*12} {'-'*40}")
    for cls, bases, is_if, has_impl, line, path, sub, _ in rows:
        bases_s = (bases or "-")[:24]
        if_s = "IF" if is_if else ""
        impl_s = "Yes" if has_impl else "No"
        print(f"  {cls:<30} {bases_s:<25} {if_s:>3} {impl_s:>5} {line or 0:>5} {sub:<12} {path}")

    # Show implementations
    for cls, _, _, _, _, path, _, _ in rows:
        impls = conn.execute("""
            SELECT i.class_name, fh.path AS header, fs.path AS source, i.method_count
            FROM implementations i
            JOIN files fh ON i.header_id = fh.id
            JOIN files fs ON i.source_id = fs.id
            WHERE i.class_name = ?
        """, (cls,)).fetchall()
        if impls:
            for cn, hdr, src, mc in impls:
                print(f"    -> {src} ({mc} methods)")

    conn.close()


def find_file(name):
    conn = get_conn()
    rows = conn.execute("""
        SELECT path, lang, lines, subsystem, has_qobject
        FROM files
        WHERE (name LIKE ? OR path LIKE ?) AND path LIKE 'src/%'
        ORDER BY path
    """, (f"%{name}%", f"%{name}%")).fetchall()

    if not rows:
        print(f"  No files matching '{name}'")
        return

    print(f"  Files matching '{name}' ({len(rows)}):")
    for path, lang, lines, sub, qobj in rows:
        qo = " Q_OBJECT" if qobj else ""
        print(f"    {path:<55} {lang:<5} {lines:>5}L  [{sub}]{qo}")
    conn.close()


def list_methods(class_name):
    conn = get_conn()
    rows = conn.execute("""
        SELECT m.name, m.params, m.is_pure_virtual, m.is_signal, m.is_slot,
               m.has_body, m.line_num, f.path
        FROM methods m
        JOIN files f ON m.file_id = f.id
        JOIN classes c ON m.class_id = c.id
        WHERE c.name = ?
        ORDER BY m.line_num
    """, (class_name,)).fetchall()

    if not rows:
        print(f"  No methods found for '{class_name}'")
        return

    print(f"  Methods of {class_name} ({len(rows)}):")
    for name, params, pv, sig, slot, body, line, path in rows:
        tags = []
        if pv: tags.append("pure-virtual")
        if sig: tags.append("signal")
        if slot: tags.append("slot")
        if body: tags.append("has-body")
        tag_s = f" [{', '.join(tags)}]" if tags else ""
        print(f"    L{line or 0:<5} {name}({(params or '')[:40]}){tag_s}")
    conn.close()


def find_implementors(interface_name):
    conn = get_conn()
    # Direct: classes that inherit from this interface
    rows = conn.execute("""
        SELECT c.name, f.path, c.has_impl, f.subsystem
        FROM classes c JOIN files f ON c.file_id = f.id
        WHERE c.bases LIKE ? AND c.lang = 'cpp'
        ORDER BY f.path
    """, (f"%{interface_name}%",)).fetchall()

    # Also via class_refs
    refs = conn.execute("""
        SELECT cr.from_class, f.path
        FROM class_refs cr
        JOIN classes c ON c.name = cr.from_class
        JOIN files f ON c.file_id = f.id
        WHERE cr.to_class = ? AND cr.ref_type = 'inherits'
        ORDER BY f.path
    """, (interface_name,)).fetchall()

    all_names = set()
    results = []
    for cls, path, impl, sub in rows:
        if cls not in all_names:
            all_names.add(cls)
            results.append((cls, path, impl, sub))
    for cls, path in refs:
        if cls not in all_names:
            all_names.add(cls)
            results.append((cls, path, None, ""))

    if not results:
        print(f"  No implementors found for '{interface_name}'")
        return

    print(f"  Implementors of {interface_name} ({len(results)}):")
    for cls, path, impl, sub in results:
        impl_s = " [has .cpp]" if impl else ""
        print(f"    {cls:<35} {path}{impl_s}")
    conn.close()


def find_dependents(filename):
    conn = get_conn()
    # Who includes this file?
    rows = conn.execute("""
        SELECT f.path, f.subsystem
        FROM includes i
        JOIN files f ON i.file_id = f.id
        WHERE i.included LIKE ?
        AND f.path LIKE 'src/%'
        ORDER BY f.path
    """, (f"%{filename}%",)).fetchall()

    if not rows:
        print(f"  No files include '{filename}'")
        return

    print(f"  Files that include '{filename}' ({len(rows)}):")
    for path, sub in rows:
        print(f"    [{sub:<12}] {path}")
    conn.close()


def list_subsystem(sub_name):
    conn = get_conn()
    rows = conn.execute("""
        SELECT c.name, c.is_interface, c.has_impl, f.path
        FROM classes c JOIN files f ON c.file_id = f.id
        WHERE f.subsystem = ? AND c.lang = 'cpp'
        ORDER BY c.name
    """, (sub_name,)).fetchall()

    if not rows:
        print(f"  No classes in subsystem '{sub_name}'")
        return

    print(f"  Classes in subsystem '{sub_name}' ({len(rows)}):")
    for cls, is_if, impl, path in rows:
        tag = "IF" if is_if else ("OK" if impl else "HO")
        print(f"    [{tag}] {cls:<35} {path}")

    # Subsystem deps
    deps_out = conn.execute(
        "SELECT to_subsystem, edge_count FROM subsystem_deps "
        "WHERE from_subsystem = ? ORDER BY edge_count DESC", (sub_name,)
    ).fetchall()
    deps_in = conn.execute(
        "SELECT from_subsystem, edge_count FROM subsystem_deps "
        "WHERE to_subsystem = ? ORDER BY edge_count DESC", (sub_name,)
    ).fetchall()
    if deps_out:
        print(f"\n  Depends on:")
        for to, cnt in deps_out:
            print(f"    -> {to} ({cnt} edges)")
    if deps_in:
        print(f"\n  Depended on by:")
        for frm, cnt in deps_in:
            print(f"    <- {frm} ({cnt} edges)")
    conn.close()


def who_uses(class_name):
    conn = get_conn()
    # Who inherits from this class?
    inheritors = conn.execute("""
        SELECT from_class FROM class_refs
        WHERE to_class = ? AND ref_type = 'inherits'
        ORDER BY from_class
    """, (class_name,)).fetchall()

    # Who includes files containing this class?
    class_files = conn.execute("""
        SELECT f.name FROM classes c JOIN files f ON c.file_id = f.id
        WHERE c.name = ? AND c.lang = 'cpp'
    """, (class_name,)).fetchall()

    includers = []
    for (fname,) in class_files:
        rows = conn.execute("""
            SELECT DISTINCT f.path, f.subsystem
            FROM includes i JOIN files f ON i.file_id = f.id
            WHERE i.included LIKE ? AND f.path LIKE 'src/%'
            ORDER BY f.path
        """, (f"%{fname}%",)).fetchall()
        includers.extend(rows)

    if inheritors:
        print(f"  Classes inheriting from {class_name} ({len(inheritors)}):")
        for (cls,) in inheritors:
            print(f"    {cls}")

    if includers:
        seen = set()
        unique = [(p, s) for p, s in includers if p not in seen and not seen.add(p)]
        print(f"\n  Files including {class_name}'s header ({len(unique)}):")
        for path, sub in unique[:30]:
            print(f"    [{sub:<12}] {path}")
        if len(unique) > 30:
            print(f"    ... and {len(unique) - 30} more")

    if not inheritors and not includers:
        print(f"  No usages found for '{class_name}'")
    conn.close()


def main():
    parser = argparse.ArgumentParser(description="Code Graph Quick Lookup")
    parser.add_argument("name", help="Name to search for")
    parser.add_argument("--file", action="store_true", help="Search files by name")
    parser.add_argument("--methods", action="store_true", help="List methods of a class")
    parser.add_argument("--impl", action="store_true", help="Find implementors of interface")
    parser.add_argument("--deps", action="store_true", help="Find files that include this")
    parser.add_argument("--sub", action="store_true", help="List classes in subsystem")
    parser.add_argument("--who-uses", action="store_true", help="Who uses this class?")

    args = parser.parse_args()

    if args.file:
        find_file(args.name)
    elif args.methods:
        list_methods(args.name)
    elif args.impl:
        find_implementors(args.name)
    elif args.deps:
        find_dependents(args.name)
    elif args.sub:
        list_subsystem(args.name)
    elif args.who_uses:
        who_uses(args.name)
    else:
        find_class(args.name)


if __name__ == "__main__":
    main()
