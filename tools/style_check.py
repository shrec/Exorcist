#!/usr/bin/env python3
"""
style_check.py — Coding convention checker for Exorcist IDE.

Scans source files for violations of project coding standards:
  - Bare `new` usage (must use smart pointers)
  - Bare `delete` usage (absolutely forbidden)
  - Header-only files with logic (need .cpp)
  - Missing `#pragma once` in headers
  - `using namespace` in headers
  - Raw `#ifdef _WIN32` instead of `Q_OS_WIN`

Usage:
  python tools/style_check.py              # Check all src/ and plugins/
  python tools/style_check.py src/agent/   # Check specific directory
  python tools/style_check.py src/foo.h    # Check specific file
  python tools/style_check.py --summary    # Show only summary counts
"""

import sys
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# ── Patterns ──

# Bare new: matches `new ClassName` but not in comments
RE_BARE_NEW = re.compile(r'\bnew\s+[A-Z]\w*')

# Bare delete
RE_BARE_DELETE = re.compile(r'\bdelete\b\s+')

# using namespace in headers
RE_USING_NS = re.compile(r'^\s*using\s+namespace\s+', re.MULTILINE)

# Raw platform checks instead of Q_OS_*
RE_RAW_PLATFORM = re.compile(r'#\s*if(?:def|ndef)?\s+(?:_WIN32|__linux__|__APPLE__|WIN32|_MSC_VER)\b')

# pragma once
RE_PRAGMA_ONCE = re.compile(r'#\s*pragma\s+once\b')

# Function body indicators in headers (non-trivial)
RE_FUNCTION_BODY = re.compile(
    r'(?:^|\s)(?:void|int|bool|QString|QStringList|QList|QHash|QMap|QSet|QJsonObject|QJsonArray|'
    r'QVariant|double|float|auto|std::)\s+\w+\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?{'
    , re.MULTILINE
)

# Simple return getter — allowed in headers
RE_SIMPLE_GETTER = re.compile(
    r'\{[^{}]*\breturn\s+m_\w+\s*;?\s*\}', re.DOTALL
)


class Violation:
    def __init__(self, file: str, line: int, code: str, msg: str):
        self.file = file
        self.line = line
        self.code = code
        self.msg = msg

    def __str__(self):
        return f"  [{self.code}] {self.file}:{self.line} — {self.msg}"


def check_file(path: Path, rel: str) -> list:
    """Check a single file for violations."""
    violations = []

    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return violations

    lines = text.split('\n')
    ext = path.suffix.lower()
    is_header = ext in ('.h', '.hpp')
    is_source = ext in ('.cpp', '.cc', '.cxx')

    if not (is_header or is_source):
        return violations

    # Skip third_party and build directories
    rel_lower = rel.replace('\\', '/').lower()
    if any(skip in rel_lower for skip in ['third_party/', 'build/', '_deps/', 'reservchrepos/']):
        return violations

    in_comment_block = False

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # Track block comments
        if '/*' in stripped:
            in_comment_block = True
        if '*/' in stripped:
            in_comment_block = False
            continue
        if in_comment_block:
            continue

        # Skip single-line comments and string literals for analysis
        code_part = stripped
        comment_pos = code_part.find('//')
        if comment_pos >= 0:
            code_part = code_part[:comment_pos]

        # Strip string literals to avoid false positives
        code_part = re.sub(r'"(?:[^"\\]|\\.)*"', '""', code_part)
        code_part = re.sub(r"'(?:[^'\\]|\\.)*'", "''", code_part)
        # Also strip QStringLiteral/tr() contents
        code_part = re.sub(r'(?:QStringLiteral|tr|QObject::tr)\s*\([^)]*\)', 'STR()', code_part)

        # Skip lines that are pure comments
        if stripped.startswith('//') or stripped.startswith('*'):
            continue

        # RULE: No bare `delete`
        if RE_BARE_DELETE.search(code_part):
            # Allow "= delete" (deleted functions)
            if '= delete' not in code_part:
                violations.append(Violation(rel, i, 'E001', f'Bare `delete` — FORBIDDEN: {stripped[:80]}'))

        # RULE: No bare `new`
        if RE_BARE_NEW.search(code_part):
            # Allow: make_unique, make_shared, Q_DISABLE_COPY, comments
            if not any(ok in code_part for ok in ['make_unique', 'make_shared', 'Q_DISABLE', 'Q_DECLARE']):
                # Allow: placement new
                if '::operator new' not in code_part and 'placement' not in code_part.lower():
                    violations.append(Violation(rel, i, 'W001', f'Bare `new` — use smart pointers: {stripped[:80]}'))

        # RULE: No raw platform checks
        if RE_RAW_PLATFORM.search(code_part):
            violations.append(Violation(rel, i, 'W002', f'Use Q_OS_WIN/Q_OS_MAC/Q_OS_UNIX: {stripped[:80]}'))

        # Header-specific rules
        if is_header:
            # RULE: No `using namespace` in headers
            if RE_USING_NS.match(stripped):
                violations.append(Violation(rel, i, 'W003', f'`using namespace` in header: {stripped[:80]}'))

    # Header-level checks
    if is_header:
        # RULE: Must have #pragma once
        if not RE_PRAGMA_ONCE.search(text):
            violations.append(Violation(rel, 1, 'W004', 'Missing `#pragma once`'))

        # RULE: Check for logic in headers (function bodies)
        # Find function bodies that aren't simple getters
        body_matches = list(RE_FUNCTION_BODY.finditer(text))
        non_trivial_bodies = 0
        for m in body_matches:
            # Find the matching closing brace
            start = m.start()
            # Extract a snippet around the match
            snippet = text[start:start+200]
            # Skip if it's just a simple return getter
            if RE_SIMPLE_GETTER.search(snippet):
                continue
            non_trivial_bodies += 1

        if non_trivial_bodies >= 3:
            # Check if a .cpp exists
            cpp_path = path.with_suffix('.cpp')
            if not cpp_path.exists():
                violations.append(Violation(
                    rel, 1, 'W005',
                    f'Header has {non_trivial_bodies} function bodies but no .cpp — split declaration/definition'
                ))

    return violations


def scan_directory(target: Path) -> list:
    """Scan a directory recursively for violations."""
    all_violations = []
    for f in sorted(target.rglob('*')):
        if f.is_file() and f.suffix in ('.h', '.hpp', '.cpp', '.cc', '.cxx'):
            rel = str(f.relative_to(REPO)).replace('\\', '/')
            all_violations.extend(check_file(f, rel))
    return all_violations


def scan_path(target_str: str) -> list:
    """Scan a file or directory."""
    target = REPO / target_str
    if not target.exists():
        # Try as absolute path
        target = Path(target_str)

    if not target.exists():
        print(f"Error: {target_str} not found")
        return []

    if target.is_file():
        rel = str(target.relative_to(REPO)).replace('\\', '/')
        return check_file(target, rel)
    else:
        return scan_directory(target)


def main():
    summary_only = '--summary' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--summary']

    if not args:
        # Default: scan src/ and plugins/
        targets = ['src/', 'plugins/']
    else:
        targets = args

    all_violations = []
    for t in targets:
        all_violations.extend(scan_path(t))

    # Group by severity
    errors = [v for v in all_violations if v.code.startswith('E')]
    warnings = [v for v in all_violations if v.code.startswith('W')]

    if not summary_only:
        if errors:
            print(f"══ ERRORS ({len(errors)}) ══")
            for v in errors:
                print(v)
            print()

        if warnings:
            print(f"══ WARNINGS ({len(warnings)}) ══")
            for v in warnings:
                print(v)
            print()

    # Summary
    print(f"── Summary ──")
    print(f"  Files scanned:  {len(set(v.file for v in all_violations)) if all_violations else 0} with issues")
    print(f"  Errors (E):     {len(errors)}")
    print(f"  Warnings (W):   {len(warnings)}")

    # Breakdown by code
    codes = {}
    for v in all_violations:
        codes[v.code] = codes.get(v.code, 0) + 1
    if codes:
        print(f"  Breakdown:")
        labels = {
            'E001': 'bare delete',
            'W001': 'bare new',
            'W002': 'raw platform check',
            'W003': 'using namespace in header',
            'W004': 'missing #pragma once',
            'W005': 'header-only with logic',
        }
        for code in sorted(codes):
            label = labels.get(code, code)
            print(f"    {code} ({label}): {codes[code]}")

    if errors:
        print(f"\n  ⛔ {len(errors)} error(s) must be fixed.")
        return 1
    elif warnings:
        print(f"\n  ⚠ {len(warnings)} warning(s) — review recommended.")
        return 0
    else:
        print(f"\n  ✓ Clean — no violations found.")
        return 0


if __name__ == '__main__':
    sys.exit(main())
