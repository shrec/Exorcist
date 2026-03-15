#!/usr/bin/env python3
"""
Exorcist IDE — Impact Analysis (Change Checker)
Before editing a file, see what depends on it and what might break.

Usage:
    python tools/check_deps.py src/editor/editorview.h     # Full impact analysis
    python tools/check_deps.py src/agent/agentcontroller.h  # What depends on this?
"""
import sqlite3
import sys
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent / "codegraph.db"


def main():
    if len(sys.argv) < 2:
        print("Usage: python tools/check_deps.py <file_path>")
        print("  Analyzes the impact of changing a file.")
        sys.exit(1)

    target = sys.argv[1].replace("\\", "/")
    if not DB_PATH.exists():
        print("ERROR: codegraph.db not found. Run build_codegraph.py first.")
        sys.exit(1)

    conn = sqlite3.connect(str(DB_PATH))

    # Find the target file
    file_row = conn.execute(
        "SELECT id, path, name, lang, lines, subsystem, has_qobject "
        "FROM files WHERE path = ? OR path LIKE ?",
        (target, f"%{target}")
    ).fetchone()

    if not file_row:
        print(f"  File '{target}' not found in code graph.")
        conn.close()
        sys.exit(1)

    fid, fpath, fname, lang, lines, subsystem, qobj = file_row

    print("=" * 70)
    print(f"  IMPACT ANALYSIS: {fpath}")
    print("=" * 70)
    print(f"  Subsystem: {subsystem}  |  Language: {lang}  |  Lines: {lines}  |  Q_OBJECT: {'Yes' if qobj else 'No'}")

    # Classes defined in this file
    classes = conn.execute(
        "SELECT name, bases, is_interface, has_impl FROM classes WHERE file_id = ?",
        (fid,)
    ).fetchall()
    if classes:
        print(f"\n  Classes defined here ({len(classes)}):")
        for cls, bases, is_if, impl in classes:
            tag = "interface" if is_if else ("implemented" if impl else "header-only")
            bases_s = f" : {bases}" if bases else ""
            print(f"    {cls}{bases_s}  [{tag}]")

    # Who includes this file directly?
    includers = conn.execute("""
        SELECT f.path, f.subsystem, f.lang
        FROM includes i JOIN files f ON i.file_id = f.id
        WHERE i.included LIKE ? OR i.included LIKE ?
        ORDER BY f.path
    """, (f"%{fname}", f"%/{fname}")).fetchall()

    if includers:
        # Categorize by subsystem
        by_sub = {}
        for path, sub, lang in includers:
            if path.startswith("src/"):
                by_sub.setdefault(sub, []).append(path)

        print(f"\n  Direct includers ({len(includers)} total, {sum(len(v) for v in by_sub.values())} in src/):")
        for sub in sorted(by_sub):
            print(f"    [{sub}]")
            for p in by_sub[sub]:
                print(f"      {p}")

    # Transitive: files that include those includers (2nd degree)
    if includers:
        first_names = {Path(p).name for p, _, _ in includers if p.startswith("src/")}
        second_degree = set()
        for inc_name in first_names:
            rows = conn.execute("""
                SELECT DISTINCT f.path FROM includes i
                JOIN files f ON i.file_id = f.id
                WHERE (i.included LIKE ? OR i.included LIKE ?)
                AND f.path LIKE 'src/%'
            """, (f"%{inc_name}", f"%/{inc_name}")).fetchall()
            for (p,) in rows:
                if p != fpath and p not in {x[0] for x in includers}:
                    second_degree.add(p)

        if second_degree:
            print(f"\n  Transitive dependents (2nd degree, {len(second_degree)}):")
            for p in sorted(second_degree)[:20]:
                print(f"      {p}")
            if len(second_degree) > 20:
                print(f"      ... and {len(second_degree) - 20} more")

    # Implementation pair
    impls = conn.execute("""
        SELECT fh.path, fs.path, i.class_name, i.method_count
        FROM implementations i
        JOIN files fh ON i.header_id = fh.id
        JOIN files fs ON i.source_id = fs.id
        WHERE i.header_id = ? OR i.source_id = ?
    """, (fid, fid)).fetchall()
    if impls:
        print(f"\n  Implementation pairs:")
        for hdr, src, cls, mc in impls:
            other = src if hdr == fpath else hdr
            print(f"    {cls}: {other} ({mc} methods)")

    # Classes that inherit from classes in this file
    for cls, _, _, _ in classes:
        children = conn.execute(
            "SELECT from_class FROM class_refs WHERE to_class = ? AND ref_type = 'inherits'",
            (cls,)
        ).fetchall()
        if children:
            print(f"\n  Classes inheriting from {cls} ({len(children)}):")
            for (child,) in children:
                cf = conn.execute(
                    "SELECT f.path FROM classes c JOIN files f ON c.file_id = f.id WHERE c.name = ?",
                    (child,)
                ).fetchone()
                loc = f"  ({cf[0]})" if cf else ""
                print(f"    {child}{loc}")

    # Risk assessment
    direct = len([i for i in includers if i[0].startswith("src/")])
    child_count = sum(
        conn.execute(
            "SELECT COUNT(*) FROM class_refs WHERE to_class = ? AND ref_type = 'inherits'",
            (cls,)
        ).fetchone()[0] for cls, _, _, _ in classes
    )

    print(f"\n  {'='*50}")
    print(f"  RISK ASSESSMENT:")
    if direct > 20:
        print(f"    [HIGH] {direct} direct dependents — changes here have wide blast radius")
    elif direct > 5:
        print(f"    [MED]  {direct} direct dependents — moderate impact")
    else:
        print(f"    [LOW]  {direct} direct dependents — localized change")

    if child_count > 0:
        print(f"    [!!]   {child_count} classes inherit from types defined here")
        print(f"           Changing virtual methods or signatures will break subclasses")

    if qobj:
        print(f"    [MOC]  File uses Q_OBJECT — signal/slot changes need moc rebuild")

    has_interface = any(is_if for _, _, is_if, _ in classes)
    if has_interface:
        print(f"    [API]  Contains interface definitions — changes affect all implementors")

    print(f"  {'='*50}")
    conn.close()


if __name__ == "__main__":
    main()
