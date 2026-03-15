import sqlite3
conn = sqlite3.connect('tools/codegraph.db')

# Test exact query from detect_features
patterns = [
    "%inlinechat%", "%chatpanel%", "%pluginmanager%", 
    "%commandpalette%", "%thememanager%", "%serviceregistry%",
    "%lspclient%", "%terminal%"
]
for pat in patterns:
    hdr = conn.execute(
        "SELECT path FROM files WHERE lower(path) LIKE ? AND ext = '.h' LIMIT 1",
        (pat,)
    ).fetchone()
    src = conn.execute(
        "SELECT path FROM files WHERE lower(path) LIKE ? AND ext = '.cpp' LIMIT 1",
        (pat,)
    ).fetchone()
    print(f"Pattern: {pat:30s} hdr={hdr[0] if hdr else 'NONE':50s} src={src[0] if src else 'NONE'}")

# Check classes table for has_impl
print("\nClasses with has_impl=1:")
for r in conn.execute("SELECT name, has_impl FROM classes WHERE has_impl = 1 AND lang='cpp' LIMIT 10"):
    print(f"  {r[0]:30s} impl={r[1]}")

print("\nTotal classes with impl:", conn.execute("SELECT COUNT(*) FROM classes WHERE has_impl=1").fetchone()[0])
print("Total classes:", conn.execute("SELECT COUNT(*) FROM classes").fetchone()[0])

conn.close()
