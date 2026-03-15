#!/usr/bin/env python3
"""
Exorcist IDE — Quick Gap Report
Queries codegraph.db for actionable implementation gaps.

Usage:
    python tools/gap_report.py
"""
import sqlite3
import sys
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent / "codegraph.db"


def main():
    if not DB_PATH.exists():
        print(f"ERROR: {DB_PATH} not found. Run build_codegraph.py first.")
        sys.exit(1)

    conn = sqlite3.connect(str(DB_PATH))

    print("=" * 70)
    print("  EXORCIST IDE - GAP REPORT")
    print("=" * 70)

    # 1. Feature gaps
    gaps = conn.execute(
        "SELECT name, status, header_file, class_name FROM features "
        "WHERE status != 'implemented' ORDER BY status, name"
    ).fetchall()
    print(f"\n  Feature gaps ({len(gaps)}):")
    for name, status, hdr, cls in gaps:
        icon = {"header-only": "[HO]", "stub": "[ST]", "missing": "[--]"}.get(status, "[??]")
        hdr_s = hdr or "-"
        if status == "header-only" and hdr:
            action = f"Create {hdr.rsplit('.', 1)[0]}.cpp"
        elif status == "stub":
            action = "Flesh out implementation"
        else:
            action = "Design + implement from scratch"
        print(f"    {icon} {name:<25} {action}")

    # 2. QObject classes without .cpp
    qobj = conn.execute("""
        SELECT c.name, f.path FROM classes c
        JOIN files f ON c.file_id = f.id
        WHERE c.has_impl = 0 AND c.is_interface = 0
        AND f.has_qobject = 1 AND f.ext = '.h'
        AND f.path LIKE 'src/%' AND c.lang = 'cpp'
        ORDER BY f.path
    """).fetchall()
    print(f"\n  QObject classes without .cpp ({len(qobj)}):")
    for cls, path in qobj:
        print(f"    {cls:<35} {path}")

    # 3. Orphan interfaces
    orphans = conn.execute("""
        SELECT c.name, f.path FROM classes c
        JOIN files f ON c.file_id = f.id
        WHERE c.is_interface = 1 AND f.path LIKE 'src/%'
        AND NOT EXISTS (
            SELECT 1 FROM class_refs cr
            WHERE cr.to_class = c.name AND cr.ref_type = 'inherits'
        )
        ORDER BY f.path
    """).fetchall()
    print(f"\n  Interfaces without known implementors ({len(orphans)}):")
    for cls, path in orphans:
        print(f"    {cls:<35} {path}")

    # 4. Subsystem health
    subs = conn.execute("""
        SELECT name, class_count, h_only_count,
            CASE WHEN class_count > 0
                THEN ROUND(100.0 * h_only_count / class_count, 1)
                ELSE 0 END AS pct
        FROM subsystems WHERE class_count > 3
        ORDER BY pct DESC
    """).fetchall()
    print(f"\n  Subsystem health (header-only ratio):")
    print(f"  {'Subsystem':<20} {'Classes':>8} {'H-Only':>8} {'Ratio':>8}")
    print(f"  {'-'*20} {'-'*8} {'-'*8} {'-'*8}")
    for name, cls_c, ho_c, pct in subs:
        flag = " <--" if pct > 40 else ""
        print(f"  {name:<20} {cls_c:>8} {ho_c:>8} {pct:>7.1f}%{flag}")

    # 5. Summary
    print(f"\n  {'='*50}")
    print(f"  Priority actions:")
    print(f"    1. Fix {len(qobj)} QObject classes missing .cpp (moc issues)")
    ho_count = len([g for g in gaps if g[1] == "header-only"])
    st_count = len([g for g in gaps if g[1] == "stub"])
    ms_count = len([g for g in gaps if g[1] == "missing"])
    print(f"    2. Create .cpp for {ho_count} header-only features")
    print(f"    3. Flesh out {st_count} stub features")
    print(f"    4. Design {ms_count} missing features")
    print(f"    5. Consider implementing {len(orphans)} orphan interfaces")
    print(f"  {'='*50}")

    conn.close()


if __name__ == "__main__":
    main()
