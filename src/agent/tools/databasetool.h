#pragma once

#include "../itool.h"

class QSqlDatabase;
class QSqlQuery;

// ── DatabaseTool ─────────────────────────────────────────────────────────────
// Execute SQL queries against SQLite databases in the workspace.
// Supports read-only queries by default for safety;
// write operations require explicit opt-in.
//
// Security: Only opens .db/.sqlite/.sqlite3 files within the workspace.
// No remote database connections (prevents SSRF).

class DatabaseTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static ToolExecResult listTables(const QSqlDatabase &db);
    static ToolExecResult showSchema(const QSqlDatabase &db);
    static ToolExecResult executeQuery(const QSqlDatabase &db, const QJsonObject &args);
    static ToolExecResult executeStatement(const QSqlDatabase &db, const QJsonObject &args);
    static ToolExecResult formatQueryResults(QSqlQuery &q, int maxRows);

    QString m_workspaceRoot;
};
