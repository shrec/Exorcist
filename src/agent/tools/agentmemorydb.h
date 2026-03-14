#pragma once

#include "../itool.h"

class QSqlQuery;

// ── AgentMemoryDbTool ─────────────────────────────────────────────────────────
// Agent's private SQLite database manager: create, delete, list, and query
// multiple databases in <app_dir>/agentmemory/.
// Each database is a separate .db file with a .json metadata sidecar.
// Like the Lua tool store pattern — persistent across sessions.
//
// Operations:
//   create_database  — create a new named database with description
//   delete_database  — delete a database and its metadata
//   list_databases   — list all databases with metadata
//   info             — show database details (tables, size, metadata)
//   query            — run SELECT on a named database
//   execute          — run DDL/DML on a named database
//   tables           — list tables in a named database
//   schema           — show CREATE statements for a named database

class AgentMemoryDbTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString storageDir();
    static QString sanitizeName(const QString &name);
    static QString dbPath(const QString &safeName);
    static QString metaPath(const QString &safeName);

    ToolExecResult doCreateDatabase(const QString &name, const QString &description);
    ToolExecResult doDeleteDatabase(const QString &name);
    ToolExecResult doListDatabases();
    ToolExecResult doDatabaseInfo(const QString &name);
    ToolExecResult doQuery(const QString &name, const QString &sql, int maxRows);
    ToolExecResult doExecute(const QString &name, const QString &sql);
    ToolExecResult doListTables(const QString &name);
    ToolExecResult doSchema(const QString &name, const QString &table);

    static QString formatResults(QSqlQuery &q, int maxRows);
};
