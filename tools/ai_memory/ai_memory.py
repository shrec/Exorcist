#!/usr/bin/env python3
"""
AI Working Memory — persistent SQLite-backed memory for AI agents.

A standalone tool the AI agent uses at its own discretion to store, recall,
and search facts, decisions, observations, and context across sessions.

Usage:
    python3 tools/ai_memory/ai_memory.py store <key> <value> [--tag T ...]
    python3 tools/ai_memory/ai_memory.py recall <key>
    python3 tools/ai_memory/ai_memory.py search <query> [--tag T] [--limit N]
    python3 tools/ai_memory/ai_memory.py list [--tag T] [--limit N] [--scope S]
    python3 tools/ai_memory/ai_memory.py delete <key>
    python3 tools/ai_memory/ai_memory.py context <topic> [--limit N]
    python3 tools/ai_memory/ai_memory.py tags
    python3 tools/ai_memory/ai_memory.py stats
    python3 tools/ai_memory/ai_memory.py gc [--days N]
    python3 tools/ai_memory/ai_memory.py export [--format json|text]
    python3 tools/ai_memory/ai_memory.py import <file>

Scopes:
    persistent  Survives across sessions (default for store)
    session     Scoped to current working session
    project     Scoped to a specific project/repo

Tags: free-form labels for categorizing memories.
    Common: decision, observation, bug, fix, pattern, preference,
            benchmark, config, TODO, blocker, risk, insight

The database auto-creates at tools/ai_memory/ai_memory.db
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sqlite3
import sys
import textwrap
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
DB_PATH = SCRIPT_DIR / "ai_memory.db"

# ---------------------------------------------------------------------------
# Schema
# ---------------------------------------------------------------------------
_SCHEMA = """
CREATE TABLE IF NOT EXISTS memories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    key         TEXT    NOT NULL UNIQUE,
    value       TEXT    NOT NULL,
    tags        TEXT    DEFAULT '',
    scope       TEXT    DEFAULT 'persistent'
                        CHECK(scope IN ('persistent','session','project')),
    project     TEXT    DEFAULT '',
    created_at  TEXT    NOT NULL,
    updated_at  TEXT    NOT NULL,
    access_count INTEGER DEFAULT 0,
    last_accessed TEXT  DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_memories_key   ON memories(key);
CREATE INDEX IF NOT EXISTS idx_memories_scope ON memories(scope);
CREATE INDEX IF NOT EXISTS idx_memories_tags  ON memories(tags);

CREATE VIRTUAL TABLE IF NOT EXISTS memories_fts
    USING fts5(key, value, tags, content=memories, content_rowid=id);

-- Triggers to keep FTS in sync
CREATE TRIGGER IF NOT EXISTS memories_ai AFTER INSERT ON memories BEGIN
    INSERT INTO memories_fts(rowid, key, value, tags)
    VALUES (new.id, new.key, new.value, new.tags);
END;

CREATE TRIGGER IF NOT EXISTS memories_ad AFTER DELETE ON memories BEGIN
    INSERT INTO memories_fts(memories_fts, rowid, key, value, tags)
    VALUES ('delete', old.id, old.key, old.value, old.tags);
END;

CREATE TRIGGER IF NOT EXISTS memories_au AFTER UPDATE ON memories BEGIN
    INSERT INTO memories_fts(memories_fts, rowid, key, value, tags)
    VALUES ('delete', old.id, old.key, old.value, old.tags);
    INSERT INTO memories_fts(rowid, key, value, tags)
    VALUES (new.id, new.key, new.value, new.tags);
END;
"""


def _now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _connect() -> sqlite3.Connection:
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH), timeout=5)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    conn.executescript(_SCHEMA)
    return conn


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_store(args: argparse.Namespace) -> int:
    """Store or update a memory."""
    key = args.key
    value = args.value
    tags = ",".join(sorted(set(args.tag))) if args.tag else ""
    scope = args.scope or "persistent"
    project = args.project or ""
    now = _now()

    conn = _connect()
    existing = conn.execute(
        "SELECT id, value FROM memories WHERE key = ?", (key,)
    ).fetchone()

    if existing:
        conn.execute(
            """UPDATE memories SET value = ?, tags = ?, scope = ?,
               project = ?, updated_at = ? WHERE key = ?""",
            (value, tags, scope, project, now, key),
        )
        action = "updated"
    else:
        conn.execute(
            """INSERT INTO memories (key, value, tags, scope, project,
               created_at, updated_at)
               VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (key, value, tags, scope, project, now, now),
        )
        action = "stored"

    conn.commit()
    conn.close()
    print(json.dumps({"status": "ok", "action": action, "key": key}))
    return 0


def cmd_recall(args: argparse.Namespace) -> int:
    """Recall a specific memory by key."""
    conn = _connect()
    row = conn.execute(
        "SELECT * FROM memories WHERE key = ?", (args.key,)
    ).fetchone()

    if not row:
        print(json.dumps({"status": "not_found", "key": args.key}))
        conn.close()
        return 1

    # Bump access stats
    conn.execute(
        """UPDATE memories SET access_count = access_count + 1,
           last_accessed = ? WHERE key = ?""",
        (_now(), args.key),
    )
    conn.commit()

    out = {
        "key": row["key"],
        "value": row["value"],
        "tags": row["tags"].split(",") if row["tags"] else [],
        "scope": row["scope"],
        "project": row["project"],
        "created_at": row["created_at"],
        "updated_at": row["updated_at"],
        "access_count": row["access_count"] + 1,
    }
    print(json.dumps(out, indent=2))
    conn.close()
    return 0


def cmd_search(args: argparse.Namespace) -> int:
    """Full-text search across all memories."""
    conn = _connect()
    limit = args.limit or 20

    query = args.query.strip()
    if not query:
        print(json.dumps({"status": "error", "message": "empty query"}))
        conn.close()
        return 1

    # FTS search
    sql = """
        SELECT m.key, m.value, m.tags, m.scope, m.project,
               m.created_at, m.updated_at, m.access_count
        FROM memories m
        JOIN memories_fts f ON m.id = f.rowid
        WHERE memories_fts MATCH ?
    """
    params: list = [query]

    if args.tag:
        sql += " AND m.tags LIKE ?"
        params.append(f"%{args.tag}%")

    sql += " ORDER BY rank LIMIT ?"
    params.append(limit)

    rows = conn.execute(sql, params).fetchall()
    results = []
    for r in rows:
        results.append({
            "key": r["key"],
            "value": r["value"],
            "tags": r["tags"].split(",") if r["tags"] else [],
            "scope": r["scope"],
            "project": r["project"],
        })

    print(json.dumps({"count": len(results), "results": results}, indent=2))
    conn.close()
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    """List memories, optionally filtered."""
    conn = _connect()
    limit = args.limit or 50

    sql = "SELECT key, tags, scope, project, updated_at, access_count FROM memories WHERE 1=1"
    params: list = []

    if args.tag:
        sql += " AND tags LIKE ?"
        params.append(f"%{args.tag}%")
    if args.scope:
        sql += " AND scope = ?"
        params.append(args.scope)

    sql += " ORDER BY updated_at DESC LIMIT ?"
    params.append(limit)

    rows = conn.execute(sql, params).fetchall()
    results = []
    for r in rows:
        results.append({
            "key": r["key"],
            "tags": r["tags"].split(",") if r["tags"] else [],
            "scope": r["scope"],
            "project": r["project"],
            "updated_at": r["updated_at"],
            "accesses": r["access_count"],
        })

    print(json.dumps({"count": len(results), "results": results}, indent=2))
    conn.close()
    return 0


def cmd_delete(args: argparse.Namespace) -> int:
    """Delete a memory by key."""
    conn = _connect()
    cur = conn.execute("DELETE FROM memories WHERE key = ?", (args.key,))
    conn.commit()
    deleted = cur.rowcount > 0
    print(json.dumps({"status": "ok" if deleted else "not_found", "key": args.key}))
    conn.close()
    return 0 if deleted else 1


def cmd_context(args: argparse.Namespace) -> int:
    """Dump all memories related to a topic (FTS + key prefix)."""
    conn = _connect()
    limit = args.limit or 30
    topic = args.topic.strip()

    results = []
    seen_ids: set = set()

    # FTS match
    try:
        rows = conn.execute(
            """SELECT m.id, m.key, m.value, m.tags, m.scope, m.project
               FROM memories m
               JOIN memories_fts f ON m.id = f.rowid
               WHERE memories_fts MATCH ?
               ORDER BY rank LIMIT ?""",
            (topic, limit),
        ).fetchall()
        for r in rows:
            if r["id"] not in seen_ids:
                seen_ids.add(r["id"])
                results.append({
                    "key": r["key"],
                    "value": r["value"],
                    "tags": r["tags"].split(",") if r["tags"] else [],
                    "scope": r["scope"],
                })
    except sqlite3.OperationalError:
        pass

    # Key prefix match
    rows = conn.execute(
        """SELECT id, key, value, tags, scope FROM memories
           WHERE key LIKE ? ORDER BY updated_at DESC LIMIT ?""",
        (f"{topic}%", limit),
    ).fetchall()
    for r in rows:
        if r["id"] not in seen_ids:
            seen_ids.add(r["id"])
            results.append({
                "key": r["key"],
                "value": r["value"],
                "tags": r["tags"].split(",") if r["tags"] else [],
                "scope": r["scope"],
            })

    # Tag match
    rows = conn.execute(
        """SELECT id, key, value, tags, scope FROM memories
           WHERE tags LIKE ? ORDER BY updated_at DESC LIMIT ?""",
        (f"%{topic}%", limit),
    ).fetchall()
    for r in rows:
        if r["id"] not in seen_ids:
            seen_ids.add(r["id"])
            results.append({
                "key": r["key"],
                "value": r["value"],
                "tags": r["tags"].split(",") if r["tags"] else [],
                "scope": r["scope"],
            })

    print(json.dumps({"topic": topic, "count": len(results),
                       "results": results}, indent=2))
    conn.close()
    return 0


def cmd_tags(args: argparse.Namespace) -> int:
    """List all tags with counts."""
    conn = _connect()
    rows = conn.execute("SELECT tags FROM memories WHERE tags != ''").fetchall()
    tag_counts: dict[str, int] = {}
    for r in rows:
        for t in r["tags"].split(","):
            t = t.strip()
            if t:
                tag_counts[t] = tag_counts.get(t, 0) + 1

    sorted_tags = sorted(tag_counts.items(), key=lambda x: -x[1])
    print(json.dumps({"tags": [{"tag": t, "count": c} for t, c in sorted_tags]}))
    conn.close()
    return 0


def cmd_stats(args: argparse.Namespace) -> int:
    """Show memory statistics."""
    conn = _connect()
    total = conn.execute("SELECT COUNT(*) AS n FROM memories").fetchone()["n"]
    by_scope = conn.execute(
        "SELECT scope, COUNT(*) AS n FROM memories GROUP BY scope"
    ).fetchall()
    top_accessed = conn.execute(
        """SELECT key, access_count FROM memories
           ORDER BY access_count DESC LIMIT 10"""
    ).fetchall()

    scopes = {r["scope"]: r["n"] for r in by_scope}
    top = [{"key": r["key"], "accesses": r["access_count"]} for r in top_accessed]

    print(json.dumps({
        "total_memories": total,
        "by_scope": scopes,
        "db_path": str(DB_PATH),
        "db_size_kb": round(DB_PATH.stat().st_size / 1024, 1) if DB_PATH.exists() else 0,
        "top_accessed": top,
    }, indent=2))
    conn.close()
    return 0


def cmd_gc(args: argparse.Namespace) -> int:
    """Garbage-collect old session memories."""
    days = args.days or 7
    cutoff = (datetime.now(timezone.utc) - timedelta(days=days)).isoformat(timespec="seconds")
    conn = _connect()
    cur = conn.execute(
        "DELETE FROM memories WHERE scope = 'session' AND updated_at < ?",
        (cutoff,),
    )
    conn.commit()
    removed = cur.rowcount
    print(json.dumps({"status": "ok", "removed": removed, "cutoff_days": days}))
    conn.close()
    return 0


def cmd_export(args: argparse.Namespace) -> int:
    """Export all memories."""
    conn = _connect()
    rows = conn.execute(
        "SELECT key, value, tags, scope, project, created_at, updated_at, access_count FROM memories ORDER BY key"
    ).fetchall()

    fmt = args.format or "json"
    if fmt == "json":
        data = []
        for r in rows:
            data.append({
                "key": r["key"],
                "value": r["value"],
                "tags": r["tags"].split(",") if r["tags"] else [],
                "scope": r["scope"],
                "project": r["project"],
                "created_at": r["created_at"],
                "updated_at": r["updated_at"],
                "access_count": r["access_count"],
            })
        print(json.dumps(data, indent=2))
    else:
        for r in rows:
            tags = f" [{r['tags']}]" if r["tags"] else ""
            print(f"[{r['scope']}] {r['key']}{tags}")
            for line in r["value"].split("\n"):
                print(f"  {line}")
            print()

    conn.close()
    return 0


def cmd_import(args: argparse.Namespace) -> int:
    """Import memories from a JSON file."""
    path = Path(args.file)
    if not path.exists():
        print(json.dumps({"status": "error", "message": f"file not found: {path}"}))
        return 1

    with open(path) as f:
        data = json.load(f)

    if not isinstance(data, list):
        print(json.dumps({"status": "error", "message": "expected a JSON array"}))
        return 1

    conn = _connect()
    imported = 0
    now = _now()
    for item in data:
        key = item.get("key", "")
        value = item.get("value", "")
        if not key or not value:
            continue
        tags = ",".join(item.get("tags", [])) if isinstance(item.get("tags"), list) else item.get("tags", "")
        scope = item.get("scope", "persistent")
        project = item.get("project", "")
        created = item.get("created_at", now)
        updated = item.get("updated_at", now)

        conn.execute(
            """INSERT INTO memories (key, value, tags, scope, project, created_at, updated_at)
               VALUES (?, ?, ?, ?, ?, ?, ?)
               ON CONFLICT(key) DO UPDATE SET
                   value = excluded.value,
                   tags = excluded.tags,
                   scope = excluded.scope,
                   project = excluded.project,
                   updated_at = excluded.updated_at""",
            (key, value, tags, scope, project, created, updated),
        )
        imported += 1

    conn.commit()
    conn.close()
    print(json.dumps({"status": "ok", "imported": imported}))
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="AI Working Memory — persistent SQLite-backed memory",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Examples:
              %(prog)s store "ci-clang-ver" "System has clang-18, not clang-17" --tag config,ci
              %(prog)s recall "ci-clang-ver"
              %(prog)s search "clang version"
              %(prog)s context "ci"
              %(prog)s list --tag decision --limit 10
              %(prog)s tags
              %(prog)s stats
              %(prog)s gc --days 3
        """),
    )
    sub = parser.add_subparsers(dest="command")

    # store
    p = sub.add_parser("store", help="Store or update a memory")
    p.add_argument("key", help="Unique key for this memory")
    p.add_argument("value", help="Content to remember")
    p.add_argument("--tag", "-t", action="append", help="Tags (repeatable)")
    p.add_argument("--scope", choices=["persistent", "session", "project"], default="persistent")
    p.add_argument("--project", help="Project scope identifier")

    # recall
    p = sub.add_parser("recall", help="Recall a memory by key")
    p.add_argument("key")

    # search
    p = sub.add_parser("search", help="Full-text search memories")
    p.add_argument("query")
    p.add_argument("--tag", "-t", help="Filter by tag")
    p.add_argument("--limit", "-n", type=int, default=20)

    # list
    p = sub.add_parser("list", help="List memories")
    p.add_argument("--tag", "-t", help="Filter by tag")
    p.add_argument("--scope", choices=["persistent", "session", "project"])
    p.add_argument("--limit", "-n", type=int, default=50)

    # delete
    p = sub.add_parser("delete", help="Delete a memory")
    p.add_argument("key")

    # context
    p = sub.add_parser("context", help="Dump all memories for a topic")
    p.add_argument("topic")
    p.add_argument("--limit", "-n", type=int, default=30)

    # tags
    sub.add_parser("tags", help="List all tags with counts")

    # stats
    sub.add_parser("stats", help="Show statistics")

    # gc
    p = sub.add_parser("gc", help="Garbage-collect old session memories")
    p.add_argument("--days", type=int, default=7)

    # export
    p = sub.add_parser("export", help="Export all memories")
    p.add_argument("--format", choices=["json", "text"], default="json")

    # import
    p = sub.add_parser("import", help="Import from JSON file")
    p.add_argument("file")

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 1

    dispatch = {
        "store": cmd_store,
        "recall": cmd_recall,
        "search": cmd_search,
        "list": cmd_list,
        "delete": cmd_delete,
        "context": cmd_context,
        "tags": cmd_tags,
        "stats": cmd_stats,
        "gc": cmd_gc,
        "export": cmd_export,
        "import": cmd_import,
    }
    return dispatch[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
