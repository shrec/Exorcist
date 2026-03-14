#pragma once

#include "../itool.h"

#include <QMap>
#include <QMutex>

class QSqlQuery;

// ── DbConnectionConfig ───────────────────────────────────────────────────────
// Stored in-memory only (never on disk). Passwords are session-scoped.

struct DbConnectionConfig
{
    QString driver;     // Qt driver name: QSQLITE, QMYSQL, QPSQL, QODBC, etc.
    QString host;
    int     port = 0;
    QString database;   // DB name or file path for SQLite
    QString user;
    QString password;
};

// ── ProjectDatabaseTool ──────────────────────────────────────────────────────
// Connect to and query project databases: MySQL, MariaDB, PostgreSQL, SQLite,
// SQL Server (via ODBC), Oracle, etc.
//
// Operations:
//   connect          — register a named connection
//   disconnect       — remove a named connection
//   query            — run a SELECT and return rows
//   execute          — run INSERT/UPDATE/DELETE/CREATE/DROP
//   tables           — list tables
//   schema           — describe table structure
//   list_connections  — show active connection names
//   list_drivers     — show available Qt SQL drivers
//
// Security: passwords are kept in-memory only, never logged or persisted.

class ProjectDatabaseTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult doConnect(const QJsonObject &args);
    ToolExecResult doDisconnect(const QString &connId);
    ToolExecResult doQuery(const QString &connId, const QString &sql, int maxRows);
    ToolExecResult doExecute(const QString &connId, const QString &sql);
    ToolExecResult doListTables(const QString &connId);
    ToolExecResult doSchema(const QString &connId, const QString &table);
    ToolExecResult doListConnections();
    ToolExecResult doListDrivers();

    bool openConnection(const DbConnectionConfig &cfg, const QString &qtConnName,
                        QString *errorOut);
    static QString resolveDriver(const QString &dbType);
    static QString formatResults(QSqlQuery &q, int maxRows);

    mutable QMutex m_mutex;
    QMap<QString, DbConnectionConfig> m_configs;
};
