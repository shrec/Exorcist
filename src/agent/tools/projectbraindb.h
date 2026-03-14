#pragma once

#include "../itool.h"

#include <QSqlDatabase>

// ── ProjectBrainDbTool ───────────────────────────────────────────────────────
// Workspace-scoped SQLite code index at .exorcist/project.db.
// Provides structured storage for file registry, symbols, dependencies,
// architectural modules, and project knowledge — replacing ad-hoc greps
// with indexed SQL queries.
//
// Operations:
//   init    — create schema + full workspace scan (first time)
//   scan    — incremental: hash comparison + rescan changed files only
//   query   — run SELECT against the index database
//   learn   — insert/update knowledge facts
//   forget  — delete knowledge entries by key
//   status  — overview (file count, symbol count, staleness)
//   execute — run arbitrary DDL/DML (for schema extensions)

class ProjectBrainDbTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString dbPath() const;
    bool ensureDir() const;

    ToolExecResult doInit();
    ToolExecResult doScan();
    ToolExecResult doQuery(const QString &sql, int maxRows);
    ToolExecResult doLearn(const QString &key, const QString &value,
                           const QString &category);
    ToolExecResult doForget(const QString &key);
    ToolExecResult doStatus();
    ToolExecResult doExecute(const QString &sql);

    bool createSchema(QSqlDatabase &db);
    int scanFiles(QSqlDatabase &db, bool fullScan);
    void scanSingleFile(QSqlDatabase &db, const QString &relPath,
                        const QString &absPath);
    void extractSymbols(QSqlDatabase &db, const QString &filePath,
                        const QString &content, const QString &language);
    void extractIncludes(QSqlDatabase &db, const QString &filePath,
                         const QString &content);
    QString detectLanguage(const QString &path) const;
    bool shouldSkipDir(const QString &name) const;

    static QString formatResults(QSqlQuery &q, int maxRows);

    QString m_workspaceRoot;
};
