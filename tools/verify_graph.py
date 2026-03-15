#!/usr/bin/env python3
"""
Exorcist IDE — Code Graph Verifier
Validates the integrity and completeness of codegraph.db.

Usage:
    python tools/verify_graph.py
"""
import sqlite3
import sys
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent / "codegraph.db"


def verify():
    if not DB_PATH.exists():
        print(f"ERROR: {DB_PATH} not found. Run build_codegraph.py first.")
        sys.exit(1)

    conn = sqlite3.connect(str(DB_PATH))
    errors = []
    warnings = []

    # ── Table existence ──
    tables = {r[0] for r in conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table'"
    ).fetchall()}
    required = {"files", "classes", "methods", "includes", "namespaces",
                "features", "implementations", "subsystems", "subsystem_deps", "class_refs"}
    missing_tables = required - tables
    if missing_tables:
        errors.append(f"Missing tables: {missing_tables}")

    # ── Row counts ──
    counts = {}
    for t in required & tables:
        counts[t] = conn.execute(f"SELECT COUNT(*) FROM [{t}]").fetchone()[0]

    print("Table row counts:")
    for t in sorted(counts):
        cnt = counts[t]
        print(f"  {t:<20} {cnt:>8}")
        if cnt == 0 and t not in ("namespaces",):
            errors.append(f"Table '{t}' is empty")

    # ── Minimum thresholds ──
    thresholds = {"files": 1000, "classes": 500, "implementations": 100, "features": 30}
    for t, minimum in thresholds.items():
        if t in counts and counts[t] < minimum:
            warnings.append(f"Table '{t}' has only {counts[t]} rows (expected >= {minimum})")

    # ── Feature status sanity ──
    feat_statuses = conn.execute(
        "SELECT status, COUNT(*) FROM features GROUP BY status"
    ).fetchall()
    print("\nFeature status distribution:")
    for status, cnt in feat_statuses:
        print(f"  {status:<15} {cnt:>4}")
        valid = {"implemented", "header-only", "stub", "missing", "source-only"}
        if status not in valid:
            errors.append(f"Unknown feature status: '{status}'")

    # ── Implementation integrity ──
    orphan_impls = conn.execute("""
        SELECT i.class_name FROM implementations i
        LEFT JOIN classes c ON c.name = i.class_name
        WHERE c.id IS NULL
    """).fetchall()
    if orphan_impls:
        warnings.append(f"{len(orphan_impls)} implementations reference non-existent classes")

    # ── Subsystem data ──
    sub_count = conn.execute("SELECT COUNT(*) FROM subsystems").fetchone()[0]
    if sub_count < 10:
        warnings.append(f"Only {sub_count} subsystems detected (expected >= 10)")

    # ── Classes with has_impl flag vs implementations table ──
    impl_flag = conn.execute("SELECT COUNT(*) FROM classes WHERE has_impl = 1").fetchone()[0]
    impl_table = conn.execute("SELECT COUNT(DISTINCT class_name) FROM implementations").fetchone()[0]
    print(f"\nImplementation consistency:")
    print(f"  has_impl=1 classes: {impl_flag}")
    print(f"  Distinct in implementations table: {impl_table}")
    if abs(impl_flag - impl_table) > impl_table * 0.2:
        warnings.append(f"Implementation flag/table mismatch: {impl_flag} vs {impl_table}")

    # ── File subsystem coverage ──
    no_sub = conn.execute(
        "SELECT COUNT(*) FROM files WHERE subsystem = '' AND path LIKE 'src/%'"
    ).fetchone()[0]
    if no_sub > 0:
        warnings.append(f"{no_sub} src/ files have no subsystem assigned")

    # ── Dependency graph sanity ──
    self_deps = conn.execute(
        "SELECT COUNT(*) FROM subsystem_deps WHERE from_subsystem = to_subsystem"
    ).fetchone()[0]
    if self_deps > 0:
        errors.append(f"{self_deps} self-referencing subsystem dependencies")

    # ── Report ──
    print(f"\n{'='*50}")
    if errors:
        print(f"  ERRORS ({len(errors)}):")
        for e in errors:
            print(f"    [ERR] {e}")
    if warnings:
        print(f"  WARNINGS ({len(warnings)}):")
        for w in warnings:
            print(f"    [WRN] {w}")
    if not errors and not warnings:
        print("  ALL CHECKS PASSED")
    print(f"{'='*50}")

    conn.close()
    return len(errors) == 0


if __name__ == "__main__":
    ok = verify()
    sys.exit(0 if ok else 1)
