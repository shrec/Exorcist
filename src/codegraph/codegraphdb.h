#pragma once

#include <QSqlDatabase>
#include <QString>

// ── CodeGraphDb ──────────────────────────────────────────────────────────────
// SQLite database layer for the Code Graph.
// Manages connection lifecycle and schema creation.
// Schema mirrors tools/build_codegraph.py (18 tables + FTS5).
//
// Usage:
//   CodeGraphDb db;
//   db.open(CodeGraphDb::defaultPath(workspaceRoot));
//   db.createSchema();
//   // ... use db.database() for indexing/queries ...
//   db.close();

class CodeGraphDb
{
public:
    ~CodeGraphDb();

    bool open(const QString &dbPath);
    void close();
    bool isOpen() const;

    bool createSchema();
    bool hasFts5() const { return m_hasFts5; }

    QSqlDatabase &database();

    static QString defaultPath(const QString &workspaceRoot);

private:
    QSqlDatabase m_db;
    QString m_connectionName;
    bool m_hasFts5 = false;
};
