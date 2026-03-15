#!/usr/bin/env python3
"""
todo_extract.py — Extract TODO/FIXME/HACK/XXX comments from the codebase.

Groups by subsystem, shows file:line, and provides a summary.

Usage:
  python tools/todo_extract.py                 # Scan src/ and plugins/
  python tools/todo_extract.py src/agent/      # Scan specific directory
  python tools/todo_extract.py --tag FIXME     # Filter by tag
  python tools/todo_extract.py --summary       # Show counts only
  python tools/todo_extract.py --priority      # Sort by priority (FIXME > HACK > TODO)
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

REPO = Path(__file__).resolve().parent.parent

# Tags in priority order
TAGS = ['FIXME', 'HACK', 'XXX', 'TODO', 'WORKAROUND', 'TEMP', 'DEPRECATED']
TAG_PRIORITY = {tag: i for i, tag in enumerate(TAGS)}

RE_TODO = re.compile(
    r'(?://|/\*|#)\s*(' + '|'.join(TAGS) + r')\b[:\s]*(.*)',
    re.IGNORECASE
)

SKIP_DIRS = {'build', 'build-llvm', 'build-ci', 'build-release', 'third_party',
             '_deps', 'ReserchRepos', 'node_modules', '.git', 'CMakeFiles'}

EXTENSIONS = {'.h', '.hpp', '.cpp', '.cc', '.cxx', '.py', '.cmake', '.txt', '.md', '.ts', '.js'}


class TodoItem:
    def __init__(self, file: str, line: int, tag: str, text: str, subsystem: str):
        self.file = file
        self.line = line
        self.tag = tag.upper()
        self.text = text.strip()
        self.subsystem = subsystem
        self.priority = TAG_PRIORITY.get(self.tag, 99)


def detect_subsystem(rel_path: str) -> str:
    parts = rel_path.replace('\\', '/').split('/')
    if len(parts) >= 2 and parts[0] == 'src':
        return parts[1]
    if len(parts) >= 2 and parts[0] == 'plugins':
        return f"plugin-{parts[1]}"
    if parts[0] == 'server':
        return 'server'
    if parts[0] == 'tests':
        return 'tests'
    if parts[0] == 'tools':
        return 'tools'
    return parts[0]


def scan_file(path: Path, rel: str, tag_filter: str = None) -> list:
    items = []
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except Exception:
        return items

    subsystem = detect_subsystem(rel)

    for i, line in enumerate(text.split('\n'), 1):
        m = RE_TODO.search(line)
        if m:
            tag = m.group(1).upper()
            if tag_filter and tag != tag_filter.upper():
                continue
            msg = m.group(2).strip().rstrip('*/')
            items.append(TodoItem(rel, i, tag, msg, subsystem))

    return items


def scan_directory(target: Path, tag_filter: str = None) -> list:
    all_items = []
    for f in sorted(target.rglob('*')):
        if f.is_file() and f.suffix in EXTENSIONS:
            # Skip build/third_party dirs
            parts = f.relative_to(REPO).parts
            if any(p in SKIP_DIRS for p in parts):
                continue
            rel = str(f.relative_to(REPO)).replace('\\', '/')
            all_items.extend(scan_file(f, rel, tag_filter))
    return all_items


def main():
    args = sys.argv[1:]
    tag_filter = None
    summary_only = False
    priority_sort = False
    targets = []

    i = 0
    while i < len(args):
        if args[i] == '--tag' and i + 1 < len(args):
            tag_filter = args[i + 1]
            i += 2
        elif args[i] == '--summary':
            summary_only = True
            i += 1
        elif args[i] == '--priority':
            priority_sort = True
            i += 1
        else:
            targets.append(args[i])
            i += 1

    if not targets:
        targets = ['src/', 'plugins/']

    all_items = []
    for t in targets:
        target = REPO / t
        if not target.exists():
            print(f"Warning: {t} not found, skipping")
            continue
        if target.is_file():
            rel = str(target.relative_to(REPO)).replace('\\', '/')
            all_items.extend(scan_file(target, rel, tag_filter))
        else:
            all_items.extend(scan_directory(target, tag_filter))

    if priority_sort:
        all_items.sort(key=lambda x: (x.priority, x.subsystem, x.file, x.line))
    else:
        all_items.sort(key=lambda x: (x.subsystem, x.file, x.line))

    # Group by subsystem
    by_subsystem = defaultdict(list)
    for item in all_items:
        by_subsystem[item.subsystem].append(item)

    if not summary_only:
        for sub in sorted(by_subsystem):
            items = by_subsystem[sub]
            print(f"══ {sub} ({len(items)}) ══")
            for item in items:
                tag_str = f"[{item.tag}]"
                print(f"  {tag_str:<12s} {item.file}:{item.line}  {item.text[:90]}")
            print()

    # Summary
    tag_counts = defaultdict(int)
    sub_counts = defaultdict(int)
    for item in all_items:
        tag_counts[item.tag] += 1
        sub_counts[item.subsystem] += 1

    print(f"── Summary ({len(all_items)} items) ──")
    if tag_counts:
        print(f"  By tag:")
        for tag in TAGS:
            if tag in tag_counts:
                print(f"    {tag:<15s} {tag_counts[tag]:>4d}")
    if sub_counts:
        print(f"  By subsystem (top 10):")
        for sub, count in sorted(sub_counts.items(), key=lambda x: -x[1])[:10]:
            print(f"    {sub:<20s} {count:>4d}")

    # Highlight urgent items
    urgent = [item for item in all_items if item.tag in ('FIXME', 'HACK', 'XXX')]
    if urgent:
        print(f"\n  Priority items (FIXME/HACK/XXX): {len(urgent)}")


if __name__ == '__main__':
    main()
