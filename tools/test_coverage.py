#!/usr/bin/env python3
"""
test_coverage.py — Map test coverage gaps across the Exorcist IDE codebase.

Cross-references tests/ files with src/ classes to identify:
  - Which classes have dedicated tests and which don't
  - Which subsystems have the best/worst coverage
  - Which tests exist and what they likely cover

Usage:
  python tools/test_coverage.py                # Full coverage report
  python tools/test_coverage.py <subsystem>    # Coverage for specific subsystem
  python tools/test_coverage.py --untested     # Show only untested classes
  python tools/test_coverage.py --tests        # List all test files + what they test
"""

import sqlite3
import re
import sys
from pathlib import Path
from collections import defaultdict

REPO = Path(__file__).resolve().parent.parent
DB = REPO / 'tools' / 'codegraph.db'
TESTS_DIR = REPO / 'tests'


def get_conn():
    if not DB.exists():
        print(f"Error: {DB} not found. Run: python tools/build_codegraph.py")
        sys.exit(1)
    return sqlite3.connect(str(DB))


def parse_test_files():
    """Parse test files to extract which classes/components they test."""
    test_map = {}  # test_file -> set of class names mentioned
    if not TESTS_DIR.exists():
        return test_map

    for f in sorted(TESTS_DIR.glob('test_*.cpp')):
        name = f.stem  # e.g., test_piecetable
        component = name.replace('test_', '')  # e.g., piecetable

        try:
            text = f.read_text(encoding='utf-8', errors='replace')
        except Exception:
            continue

        # Extract #include'd headers to find what's being tested
        includes = set()
        for m in re.finditer(r'#include\s+[<"]([^>"]+)[>"]', text):
            inc = m.group(1)
            # Extract class name from header path
            hdr = Path(inc).stem
            includes.add(hdr)

        # Extract class names instantiated or referenced in the test
        classes = set()
        for m in re.finditer(r'\b([A-Z][a-zA-Z0-9]+)\b', text):
            cls = m.group(1)
            # Filter out common Qt/test macros and types
            if cls in ('QObject', 'QTest', 'QTEST', 'QCOMPARE', 'QVERIFY', 'QFETCH',
                        'QBENCHMARK', 'QString', 'QStringList', 'QList', 'QHash',
                        'QMap', 'QSet', 'QVariant', 'QJsonObject', 'QJsonArray',
                        'QJsonDocument', 'QFile', 'QDir', 'QTimer', 'QSignalSpy',
                        'QCoreApplication', 'QApplication', 'QWidget', 'QLabel',
                        'QTemporaryDir', 'QTemporaryFile', 'QByteArray', 'QTEST_MAIN',
                        'QTextCursor', 'QTextDocument', 'QPlainTextEdit', 'QChar',
                        'QTextBlock', 'QTextFormat', 'QColor', 'QFont', 'QRect',
                        'QSize', 'QPoint', 'QUrl', 'QNetworkRequest', 'QProcess',
                        'QRegularExpression', 'QDebug', 'QThread', 'QMutex',
                        'QElapsedTimer', 'QDateTime', 'SLOT', 'SIGNAL',
                        'True', 'False', 'NULL'):
                continue
            classes.add(cls)

        test_map[f.name] = {
            'component': component,
            'includes': includes,
            'classes': classes,
        }

    return test_map


def main():
    args = sys.argv[1:]
    subsystem_filter = None
    untested_only = False
    tests_only = False

    filtered_args = []
    for a in args:
        if a == '--untested':
            untested_only = True
        elif a == '--tests':
            tests_only = True
        else:
            filtered_args.append(a)

    if filtered_args:
        subsystem_filter = filtered_args[0]

    conn = get_conn()
    test_map = parse_test_files()

    # Build reverse map: class_name_lower -> test files that reference it
    class_to_tests = defaultdict(set)
    for test_file, info in test_map.items():
        for cls in info['classes']:
            class_to_tests[cls.lower()].add(test_file)
        for inc in info['includes']:
            class_to_tests[inc.lower()].add(test_file)
        # Also map component name
        class_to_tests[info['component'].lower()].add(test_file)

    if tests_only:
        print(f"══ Test Files ({len(test_map)}) ══")
        for test_file in sorted(test_map):
            info = test_map[test_file]
            incs = ', '.join(sorted(info['includes']))[:80]
            print(f"  {test_file:<45s} includes: {incs}")
        print()
        conn.close()
        return

    # Get all non-interface, non-trivial classes from src/
    query = """
        SELECT c.name, c.has_impl, c.is_interface, f.path, f.subsystem, f.lines
        FROM classes c JOIN files f ON c.file_id = f.id
        WHERE f.path LIKE 'src/%' AND c.lang = 'cpp' AND f.ext = '.h'
        AND c.is_interface = 0
    """
    if subsystem_filter:
        query += f" AND f.subsystem = '{subsystem_filter}'"
    query += " ORDER BY f.subsystem, c.name"

    classes = conn.execute(query).fetchall()

    # Categorize
    tested = []
    untested = []

    for cls in classes:
        name, has_impl, is_iface, path, subsystem, lines = cls
        # Check if any test references this class
        test_files = set()
        test_files |= class_to_tests.get(name.lower(), set())
        # Also check with common name transformations
        # e.g., PieceTable -> piecetable, SearchWorker -> searchworker
        snake = re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()
        test_files |= class_to_tests.get(snake, set())
        test_files |= class_to_tests.get(name.lower().replace('_', ''), set())

        if test_files:
            tested.append((name, subsystem, path, sorted(test_files)))
        else:
            untested.append((name, subsystem, path, lines))

    # Display
    if not untested_only:
        print(f"══ Tested Classes ({len(tested)}) ══")
        by_sub = defaultdict(list)
        for name, sub, path, tests in tested:
            by_sub[sub].append((name, tests))
        for sub in sorted(by_sub):
            items = by_sub[sub]
            print(f"  ── {sub} ({len(items)}) ──")
            for name, tests in items:
                test_str = ', '.join(tests)[:60]
                print(f"    ✓ {name:<35s} ← {test_str}")
        print()

    print(f"══ Untested Classes ({len(untested)}) ══")
    by_sub = defaultdict(list)
    for name, sub, path, lines in untested:
        by_sub[sub].append((name, path, lines))
    for sub in sorted(by_sub):
        items = by_sub[sub]
        print(f"  ── {sub} ({len(items)}) ──")
        for name, path, lines in items:
            print(f"    · {name:<35s} {lines:>5d}L  {path}")
    print()

    # Subsystem coverage summary
    sub_tested = defaultdict(int)
    sub_total = defaultdict(int)
    for name, sub, path, tests in tested:
        sub_tested[sub] += 1
        sub_total[sub] += 1
    for name, sub, path, lines in untested:
        sub_total[sub] += 1

    print(f"── Subsystem Coverage ──")
    print(f"  {'Subsystem':<20s} {'Tested':>7s} {'Total':>7s} {'Coverage':>9s}")
    print(f"  {'─'*20} {'─'*7} {'─'*7} {'─'*9}")
    for sub in sorted(sub_total, key=lambda s: sub_tested.get(s, 0) / max(sub_total[s], 1), reverse=True):
        t = sub_tested.get(sub, 0)
        total = sub_total[sub]
        pct = (t / total * 100) if total > 0 else 0
        bar = '█' * int(pct / 10) + '░' * (10 - int(pct / 10))
        print(f"  {sub:<20s} {t:>7d} {total:>7d} {pct:>7.1f}% {bar}")

    total_t = sum(sub_tested.values())
    total_all = sum(sub_total.values())
    pct = (total_t / total_all * 100) if total_all > 0 else 0
    print(f"  {'─'*20} {'─'*7} {'─'*7} {'─'*9}")
    print(f"  {'TOTAL':<20s} {total_t:>7d} {total_all:>7d} {pct:>7.1f}%")

    # High-value untested targets (large classes with implementations)
    high_value = [(n, s, p, l) for n, s, p, l in untested if l > 50]
    if high_value:
        high_value.sort(key=lambda x: -x[3])
        print(f"\n── High-Value Untested (>50 lines, top 15) ──")
        for name, sub, path, lines in high_value[:15]:
            print(f"  {name:<35s} {lines:>5d}L  [{sub}] {path}")

    conn.close()


if __name__ == '__main__':
    main()
