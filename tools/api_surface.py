#!/usr/bin/env python3
"""
api_surface.py — Extract the public API surface of a class or subsystem.

Shows signals, slots, public methods, and inherited interfaces. Essential for
understanding what a component exposes before integrating with it.

Usage:
  python tools/api_surface.py ClassName              # API of a single class
  python tools/api_surface.py --sub agent            # API of all classes in subsystem
  python tools/api_surface.py --sub agent --public   # Public methods only
  python tools/api_surface.py --sub agent --signals  # Signals only
  python tools/api_surface.py --file src/editor/editorview.h  # API from a header
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

REPO = Path(__file__).resolve().parent.parent


class ApiMethod:
    def __init__(self, name: str, signature: str, kind: str, access: str):
        self.name = name
        self.signature = signature  # full declaration line
        self.kind = kind            # 'method', 'signal', 'slot', 'virtual', 'override', 'static'
        self.access = access        # 'public', 'protected', 'private', 'signals'


def parse_api(header_path: Path) -> dict:
    """Parse a header file and extract class APIs."""
    try:
        text = header_path.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return {}

    result = {}  # class_name -> list of ApiMethod

    # Find class declarations
    class_pattern = re.compile(
        r'class\s+(?:\w+\s+)?(\w+)\s*(?::\s*(?:public|protected|private)\s+(\S+(?:\s*,\s*(?:public|protected|private)\s+\S+)*))?\s*\{',
        re.MULTILINE
    )

    for cm in class_pattern.finditer(text):
        class_name = cm.group(1)
        bases = cm.group(2) if cm.group(2) else ''

        # Find the class body
        start = cm.end()
        depth = 1
        pos = start
        while pos < len(text) and depth > 0:
            if text[pos] == '{':
                depth += 1
            elif text[pos] == '}':
                depth -= 1
            pos += 1

        body = text[start:pos - 1]
        methods = parse_class_body(body)
        if methods:
            result[class_name] = {
                'bases': bases,
                'methods': methods,
                'path': str(header_path.relative_to(REPO)).replace('\\', '/'),
            }

    return result


def parse_class_body(body: str) -> list:
    """Parse class body to extract method declarations with access levels."""
    methods = []
    access = 'private'  # C++ default
    depth = 0
    in_signals = False

    for line in body.split('\n'):
        stripped = line.strip()

        # Track brace depth to skip inline bodies
        for ch in stripped:
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
        if depth > 0:
            continue
        if depth < 0:
            depth = 0

        # Skip empty lines and comments
        if not stripped or stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
            continue

        # Access specifier detection
        if re.match(r'^public\s*:', stripped):
            access = 'public'
            in_signals = False
            continue
        if re.match(r'^protected\s*:', stripped):
            access = 'protected'
            in_signals = False
            continue
        if re.match(r'^private\s*:', stripped):
            access = 'private'
            in_signals = False
            continue
        if re.match(r'^signals\s*:', stripped) or re.match(r'^Q_SIGNALS\s*:', stripped):
            access = 'signals'
            in_signals = True
            continue
        if re.match(r'^(?:public|protected|private)\s+slots\s*:', stripped):
            m = re.match(r'^(public|protected|private)\s+slots\s*:', stripped)
            access = m.group(1)
            in_signals = False
            continue

        # Skip macros, typedefs, enums, using, friend, etc
        if any(stripped.startswith(kw) for kw in
               ('Q_OBJECT', 'Q_PROPERTY', 'Q_ENUM', 'Q_DECLARE', 'Q_DISABLE',
                'Q_INVOKABLE', 'using ', 'typedef ', 'friend ', 'enum ', 'struct ',
                '#', 'static_assert')):
            continue

        # Skip member variables (end with ; but no parentheses)
        if stripped.endswith(';') and '(' not in stripped:
            continue

        # Match method declarations
        # Return type + name + params + optional qualifiers + ;
        m = re.match(
            r'(?:(?:virtual|static|explicit|inline|constexpr)\s+)*'
            r'(?:[\w:*&<>,\s]+?)\s+'  # return type
            r'(~?\w+)\s*'             # method name
            r'\(([^)]*)\)\s*'          # parameters
            r'([\w\s=&*]*?)\s*;',      # qualifiers (const, override, = 0, etc.)
            stripped
        )

        if not m:
            # Try constructor/destructor pattern
            m = re.match(
                r'(?:explicit\s+)?'
                r'(~?\w+)\s*'
                r'\(([^)]*)\)\s*'
                r'([\w\s=]*?)\s*;',
                stripped
            )

        if m:
            name = m.group(1)
            params = m.group(2).strip()
            quals = m.group(3).strip()

            # Determine kind
            kind = 'method'
            if in_signals or access == 'signals':
                kind = 'signal'
            elif 'virtual' in stripped and '= 0' in quals:
                kind = 'pure-virtual'
            elif 'virtual' in stripped:
                kind = 'virtual'
            elif 'override' in quals:
                kind = 'override'
            elif 'static' in stripped:
                kind = 'static'

            # Build clean signature
            sig = stripped.rstrip(';').strip()

            methods.append(ApiMethod(name, sig, kind, access if not in_signals else 'signals'))

    return methods


def display_class_api(class_name: str, info: dict, filter_kind: str = None):
    """Display API for a single class."""
    methods = info['methods']
    bases = info['bases']
    path = info['path']

    if filter_kind:
        methods = [m for m in methods if m.kind == filter_kind or m.access == filter_kind]

    if not methods:
        return

    print(f"══ {class_name} ══")
    if bases:
        print(f"  extends: {bases}")
    print(f"  file: {path}")
    print()

    # Group by access + kind
    groups = defaultdict(list)
    for m in methods:
        key = f"{m.access} {m.kind}" if m.kind != 'method' else m.access
        groups[key].append(m)

    # Display order
    order = ['signals signal', 'public method', 'public virtual', 'public pure-virtual',
             'public override', 'public static', 'public slots method',
             'protected method', 'protected virtual', 'protected override',
             'private method']

    displayed = set()
    for key in order:
        if key in groups:
            display_group(key, groups[key])
            displayed.add(key)

    # Display any remaining groups
    for key in sorted(groups):
        if key not in displayed:
            display_group(key, groups[key])

    print()


def display_group(key: str, methods: list):
    """Display a group of methods."""
    label = key.replace('method', '').strip()
    if not label:
        label = 'public'
    print(f"  ── {label} ({len(methods)}) ──")
    for m in methods:
        kind_tag = f" [{m.kind}]" if m.kind not in ('method', 'signal') else ''
        print(f"    {m.signature}{kind_tag}")


def main():
    args = sys.argv[1:]
    subsystem = None
    file_path = None
    filter_kind = None
    class_name = None

    i = 0
    while i < len(args):
        if args[i] == '--sub' and i + 1 < len(args):
            subsystem = args[i + 1]
            i += 2
        elif args[i] == '--file' and i + 1 < len(args):
            file_path = args[i + 1]
            i += 2
        elif args[i] == '--public':
            filter_kind = 'public'
            i += 1
        elif args[i] == '--signals':
            filter_kind = 'signals'
            i += 1
        elif args[i] == '--slots':
            filter_kind = 'slots'
            i += 1
        elif args[i] == '--virtual':
            filter_kind = 'pure-virtual'
            i += 1
        else:
            class_name = args[i]
            i += 1

    if file_path:
        # Parse a specific file
        p = REPO / file_path
        if not p.exists():
            print(f"Error: {file_path} not found")
            return
        result = parse_api(p)
        for cn, info in result.items():
            display_class_api(cn, info, filter_kind)
        return

    if subsystem:
        # Parse all headers in a subsystem
        sub_dir = REPO / 'src' / subsystem
        if not sub_dir.exists():
            print(f"Error: src/{subsystem}/ not found")
            return
        total_methods = 0
        total_classes = 0
        for f in sorted(sub_dir.rglob('*.h')):
            result = parse_api(f)
            for cn, info in result.items():
                display_class_api(cn, info, filter_kind)
                total_classes += 1
                total_methods += len(info['methods'])
        print(f"── Summary: {total_classes} classes, {total_methods} API methods ──")
        return

    if class_name:
        # Find header file for this class
        import sqlite3
        db = REPO / 'tools' / 'codegraph.db'
        if not db.exists():
            # Fallback: search headers
            print("Warning: codegraph.db not found, searching headers directly...")
            for f in (REPO / 'src').rglob('*.h'):
                result = parse_api(f)
                if class_name in result:
                    display_class_api(class_name, result[class_name], filter_kind)
                    return
            print(f"Class '{class_name}' not found")
            return

        conn = sqlite3.connect(str(db))
        rows = conn.execute(
            "SELECT f.path FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE c.name = ? AND f.ext = '.h' ORDER BY f.path", (class_name,)
        ).fetchall()
        conn.close()

        if not rows:
            print(f"Class '{class_name}' not found in code graph")
            return

        for row in rows:
            p = REPO / row[0]
            if p.exists():
                result = parse_api(p)
                if class_name in result:
                    display_class_api(class_name, result[class_name], filter_kind)
        return

    # No arguments — show help
    print(__doc__)


if __name__ == '__main__':
    main()
