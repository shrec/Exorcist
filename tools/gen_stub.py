#!/usr/bin/env python3
"""
Exorcist IDE — .cpp Stub Generator
Generates a skeleton .cpp file from a .h header file.

Usage:
    python tools/gen_stub.py src/agent/telemetrymanager.h         # Generate .cpp
    python tools/gen_stub.py src/agent/telemetrymanager.h --dry   # Preview only
    python tools/gen_stub.py src/agent/chat/chatmarkdownwidget.h  # Generate .cpp
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Patterns
RE_CLASS = re.compile(
    r'^\s*(?:class|struct)\s+(?:Q_DECL_EXPORT\s+|Q_DECL_IMPORT\s+)?(\w+)'
    r'(?:\s*:\s*(?:public|protected|private)\s+([\w:,\s]+?))?\s*\{',
    re.MULTILINE
)
RE_METHOD = re.compile(
    r'^\s*((?:virtual\s+|static\s+|explicit\s+)?'
    r'(?:[\w:<>,\s\*&]+?\s+))'           # return type
    r'(\w+)\s*\(([^)]*)\)'               # name + params
    r'\s*((?:const)?(?:\s*override)?)'    # trailing qualifiers
    r'\s*;',                              # must end with semicolon (declaration only)
    re.MULTILINE
)
RE_CONSTRUCTOR = re.compile(
    r'^\s*(?:explicit\s+)?(\w+)\s*\(([^)]*)\)\s*;',
    re.MULTILINE
)
RE_DESTRUCTOR = re.compile(
    r'^\s*(?:virtual\s+)?~(\w+)\s*\(\s*\)\s*;',
    re.MULTILINE
)
RE_PURE_VIRTUAL = re.compile(r'=\s*0\s*;?\s*$')
RE_SIGNAL_SECTION = re.compile(r'signals\s*:', re.MULTILINE)
RE_QOBJECT = re.compile(r'Q_OBJECT')

# Skip these method names
SKIP_METHODS = {"emit", "connect", "disconnect", "tr", "Q_OBJECT",
                "Q_SIGNALS", "Q_SLOTS", "signals", "slots"}


def parse_header(header_path):
    """Parse a header file and extract class structures."""
    content = header_path.read_text(encoding="utf-8", errors="replace")
    classes = []

    for m in RE_CLASS.finditer(content):
        cls_name = m.group(1)
        bases = m.group(2) or ""

        # Find the class body
        start = m.end()
        brace_depth = 1
        pos = start
        while pos < len(content) and brace_depth > 0:
            if content[pos] == '{':
                brace_depth += 1
            elif content[pos] == '}':
                brace_depth -= 1
            pos += 1
        class_body = content[start:pos]

        methods = []
        constructors = []
        has_destructor = False
        has_qobject = bool(RE_QOBJECT.search(content[:start + 100]))

        # Line-by-line parsing with brace and signal tracking
        in_signals = False
        body_depth = 0    # tracks { } inside methods to skip inline bodies
        lines_list = class_body.split("\n")

        for line in lines_list:
            stripped = line.strip()

            # Track brace depth for inline bodies
            body_depth += stripped.count("{") - stripped.count("}")
            if body_depth > 0:
                continue  # inside an inline body, skip
            body_depth = max(0, body_depth)

            # Detect section changes
            if re.match(r'signals\s*:', stripped):
                in_signals = True
                continue
            if re.match(r'(public|private|protected)\s*(slots)?\s*:', stripped):
                in_signals = False
                continue

            if in_signals:
                continue  # skip everything in signals section

            # Constructor declaration: ClassName(...);
            cm = RE_CONSTRUCTOR.match(line)
            if cm and cm.group(1) == cls_name:
                constructors.append(cm.group(2).strip())
                continue

            # Destructor declaration: ~ClassName();
            dm = RE_DESTRUCTOR.match(line)
            if dm and dm.group(1) == cls_name:
                has_destructor = True
                continue

            # Method declaration: must end with ; (not { or = 0)
            mm = RE_METHOD.match(line)
            if mm:
                ret_type = mm.group(1).strip()
                name = mm.group(2).strip()
                params = mm.group(3).strip()
                trailing = mm.group(4).strip() if mm.group(4) else ""

                if name in SKIP_METHODS or name == cls_name or name.startswith("~"):
                    continue
                if "return" in ret_type or "emit" in ret_type:
                    continue
                if RE_PURE_VIRTUAL.search(stripped):
                    continue

                is_const = "const" in trailing
                is_override = "override" in trailing
                is_static = "static" in ret_type

                methods.append({
                    "ret": ret_type.replace("virtual ", "").replace("static ", "").strip(),
                    "name": name,
                    "params": params,
                    "is_const": is_const,
                    "is_override": is_override,
                    "is_static": is_static,
                })

        classes.append({
            "name": cls_name,
            "bases": bases.strip(),
            "constructors": constructors,
            "has_destructor": has_destructor,
            "methods": methods,
            "has_qobject": has_qobject,
        })

    return classes


def generate_cpp(header_path, classes):
    """Generate .cpp content from parsed class info."""
    rel_header = str(header_path.relative_to(ROOT)).replace("\\", "/")
    header_name = header_path.name

    lines = [
        f'#include "{header_name}"',
        "",
    ]

    for cls in classes:
        if not cls["constructors"] and not cls["has_destructor"] and not cls["methods"]:
            lines.append(f"// {cls['name']} — no methods to implement (data struct or interface)")
            lines.append("")
            continue

        lines.append(f"// ── {cls['name']} {'─' * max(1, 50 - len(cls['name']))}") 
        lines.append("")

        # Constructors
        for params in cls["constructors"]:
            if params:
                # Extract param names for initializer
                lines.append(f"{cls['name']}::{cls['name']}({params})")
            else:
                lines.append(f"{cls['name']}::{cls['name']}()")

            if cls["bases"]:
                base = cls["bases"].split(",")[0].strip().split()[-1]
                lines.append(f"    : {base}()")
            lines.append("{")
            lines.append("}")
            lines.append("")

        # Destructor
        if cls["has_destructor"]:
            lines.append(f"{cls['name']}::~{cls['name']}()")
            lines.append("{")
            lines.append("}")
            lines.append("")

        # Methods
        for m in cls["methods"]:
            const_s = " const" if m["is_const"] else ""
            ret = m["ret"]
            if ret == "void":
                body = "    // TODO: implement"
            elif ret == "bool":
                body = "    return false;"
            elif ret == "int" or ret == "qint64" or ret == "size_t":
                body = "    return 0;"
            elif ret == "QString":
                body = '    return {};'
            elif ret == "QStringList" or ret == "QList" in ret:
                body = "    return {};"
            elif "*" in ret:
                body = "    return nullptr;"
            else:
                body = "    return {};"

            lines.append(f"{ret} {cls['name']}::{m['name']}({m['params']}){const_s}")
            lines.append("{")
            lines.append(body)
            lines.append("}")
            lines.append("")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print("Usage: python tools/gen_stub.py <header.h> [--dry]")
        print("  Generates a skeleton .cpp from a .h file.")
        sys.exit(1)

    header_arg = sys.argv[1].replace("\\", "/")
    dry_run = "--dry" in sys.argv

    header_path = ROOT / header_arg
    if not header_path.exists():
        print(f"ERROR: {header_path} not found.")
        sys.exit(1)

    cpp_path = header_path.with_suffix(".cpp")

    if cpp_path.exists() and not dry_run:
        print(f"WARNING: {cpp_path.relative_to(ROOT)} already exists!")
        print(f"  Use --dry to preview what would be generated.")
        sys.exit(1)

    classes = parse_header(header_path)
    if not classes:
        print(f"  No classes found in {header_arg}")
        sys.exit(1)

    print(f"  Header: {header_arg}")
    print(f"  Classes found: {', '.join(c['name'] for c in classes)}")
    total_methods = sum(len(c["methods"]) + len(c["constructors"]) + (1 if c["has_destructor"] else 0) for c in classes)
    print(f"  Methods to stub: {total_methods}")

    content = generate_cpp(header_path, classes)

    if dry_run:
        print(f"\n  --- Preview of {cpp_path.name} ---")
        print(content)
    else:
        cpp_path.write_text(content, encoding="utf-8")
        print(f"  Generated: {cpp_path.relative_to(ROOT)}")
        print(f"  Don't forget to add it to the CMakeLists.txt!")


if __name__ == "__main__":
    main()
