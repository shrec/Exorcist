#include "projectdbtool.h"

#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

// ── spec ──────────────────────────────────────────────────────────────────────

ToolSpec ProjectDatabaseTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("project_database");
    s.description = QStringLiteral(
        "Connect to and query project databases. Supports MySQL, MariaDB, "
        "PostgreSQL, SQLite, SQL Server (via ODBC), and Oracle.\n\n"
        "Operations:\n"
        "  'connect'          — register a named connection (type, host, port, database, user, password)\n"
        "  'disconnect'       — remove a connection by name\n"
        "  'query'            — run a SELECT query and return rows\n"
        "  'execute'          — run INSERT/UPDATE/DELETE/CREATE/DROP statements\n"
        "  'tables'           — list all tables in the database\n"
        "  'schema'           — show table structure\n"
        "  'list_connections' — show registered connection names\n"
        "  'list_drivers'     — show available database drivers on this system\n\n"
        "Supported dbType values: sqlite, mysql, mariadb, postgres, sqlserver, oracle, odbc.\n"
        "Passwords are kept in session memory only, never logged or persisted.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: connect, disconnect, query, execute, tables, schema, list_connections, list_drivers")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("connect"), QStringLiteral("disconnect"),
                    QStringLiteral("query"), QStringLiteral("execute"),
                    QStringLiteral("tables"), QStringLiteral("schema"),
                    QStringLiteral("list_connections"), QStringLiteral("list_drivers")}}
            }},
            {QStringLiteral("connectionName"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Name of the connection to use or create.")}
            }},
            {QStringLiteral("dbType"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Database type: sqlite, mysql, mariadb, postgres, sqlserver, oracle, odbc")}
            }},
            {QStringLiteral("host"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Database host (for remote databases).")}
            }},
            {QStringLiteral("port"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"), QStringLiteral("Database port.")}
            }},
            {QStringLiteral("database"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Database name or file path (for SQLite).")}
            }},
            {QStringLiteral("user"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Database username.")}
            }},
            {QStringLiteral("password"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Database password. Never logged or persisted.")}
            }},
            {QStringLiteral("sql"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("SQL statement to execute.")}
            }},
            {QStringLiteral("table"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Table name (for schema operation).")}
            }},
            {QStringLiteral("maxRows"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum rows to return (default: 100, max: 1000).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

// ── invoke ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::invoke(const QJsonObject &args)
{
    const QString op     = args[QLatin1String("operation")].toString();
    const QString connId = args[QLatin1String("connectionName")].toString();

    if (op == QLatin1String("connect"))
        return doConnect(args);
    if (op == QLatin1String("disconnect"))
        return doDisconnect(connId);
    if (op == QLatin1String("list_connections"))
        return doListConnections();
    if (op == QLatin1String("list_drivers"))
        return doListDrivers();

    // All remaining operations require a connection name
    if (connId.isEmpty())
        return {false, {}, {},
            QStringLiteral("'connectionName' is required for '%1'.").arg(op)};

    if (op == QLatin1String("query")) {
        const int maxRows = qBound(1, args[QLatin1String("maxRows")].toInt(100), 1000);
        return doQuery(connId, args[QLatin1String("sql")].toString(), maxRows);
    }
    if (op == QLatin1String("execute"))
        return doExecute(connId, args[QLatin1String("sql")].toString());
    if (op == QLatin1String("tables"))
        return doListTables(connId);
    if (op == QLatin1String("schema"))
        return doSchema(connId, args[QLatin1String("table")].toString());

    return {false, {}, {},
        QStringLiteral("Unknown operation: '%1'.").arg(op)};
}

// ── connect ───────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doConnect(const QJsonObject &args)
{
    const QString connId = args[QLatin1String("connectionName")].toString();
    const QString dbType = args[QLatin1String("dbType")].toString().toLower();

    if (connId.isEmpty())
        return {false, {}, {},
            QStringLiteral("'connectionName' is required for 'connect'.")};
    if (dbType.isEmpty())
        return {false, {}, {},
            QStringLiteral("'dbType' is required for 'connect'. "
                           "Options: sqlite, mysql, mariadb, postgres, sqlserver, oracle, odbc")};

    const QString driver = resolveDriver(dbType);
    if (driver.isEmpty())
        return {false, {}, {},
            QStringLiteral("Unknown database type: '%1'. "
                           "Options: sqlite, mysql, mariadb, postgres, sqlserver, oracle, odbc")
                .arg(dbType)};

    if (!QSqlDatabase::isDriverAvailable(driver))
        return {false, {}, {},
            QStringLiteral("Qt SQL driver '%1' is not available on this system. "
                           "Available drivers: %2")
                .arg(driver, QSqlDatabase::drivers().join(QStringLiteral(", ")))};

    DbConnectionConfig cfg;
    cfg.driver   = driver;
    cfg.host     = args[QLatin1String("host")].toString();
    cfg.port     = args[QLatin1String("port")].toInt(0);
    cfg.database = args[QLatin1String("database")].toString();
    cfg.user     = args[QLatin1String("user")].toString();
    cfg.password = args[QLatin1String("password")].toString();

    // Test the connection
    const QString testConn = QStringLiteral("proj_test_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    QString error;
    bool ok = openConnection(cfg, testConn, &error);
    {
        QSqlDatabase::database(testConn).close();
    }
    QSqlDatabase::removeDatabase(testConn);

    if (!ok)
        return {false, {}, {},
            QStringLiteral("Connection failed: %1").arg(error)};

    // Store config (in-memory only)
    {
        QMutexLocker lock(&m_mutex);
        m_configs[connId] = cfg;
    }

    return {true, {},
        QStringLiteral("Connected '%1' (%2 @ %3). Ready for queries.")
            .arg(connId, dbType,
                 cfg.host.isEmpty() ? cfg.database : cfg.host),
        {}};
}

// ── disconnect ────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doDisconnect(const QString &connId)
{
    if (connId.isEmpty())
        return {false, {}, {},
            QStringLiteral("'connectionName' is required for 'disconnect'.")};

    QMutexLocker lock(&m_mutex);
    if (!m_configs.contains(connId))
        return {false, {}, {},
            QStringLiteral("No connection named '%1'.").arg(connId)};

    m_configs.remove(connId);
    return {true, {},
        QStringLiteral("Disconnected '%1'.").arg(connId), {}};
}

// ── query ─────────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doQuery(const QString &connId,
                                            const QString &sql, int maxRows)
{
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'query'.")};

    DbConnectionConfig cfg;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_configs.contains(connId))
            return {false, {}, {},
                QStringLiteral("No connection named '%1'. Use 'connect' first.").arg(connId)};
        cfg = m_configs[connId];
    }

    const QString qtConn = QStringLiteral("proj_%1_%2")
        .arg(connId.left(8),
             QUuid::createUuid().toString(QUuid::Id128).left(6));

    QString error;
    if (!openConnection(cfg, qtConn, &error)) {
        QSqlDatabase::removeDatabase(qtConn);
        return {false, {}, {},
            QStringLiteral("Connection error: %1").arg(error)};
    }

    ToolExecResult result;
    {
        QSqlQuery q(QSqlDatabase::database(qtConn));
        if (!q.exec(sql)) {
            result = {false, {}, {},
                QStringLiteral("SQL error: %1").arg(q.lastError().text())};
        } else {
            result = {true, {}, formatResults(q, maxRows), {}};
        }
        QSqlDatabase::database(qtConn).close();
    }
    QSqlDatabase::removeDatabase(qtConn);
    return result;
}

// ── execute ───────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doExecute(const QString &connId,
                                              const QString &sql)
{
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'execute'.")};

    DbConnectionConfig cfg;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_configs.contains(connId))
            return {false, {}, {},
                QStringLiteral("No connection named '%1'. Use 'connect' first.").arg(connId)};
        cfg = m_configs[connId];
    }

    const QString qtConn = QStringLiteral("proj_%1_%2")
        .arg(connId.left(8),
             QUuid::createUuid().toString(QUuid::Id128).left(6));

    QString error;
    if (!openConnection(cfg, qtConn, &error)) {
        QSqlDatabase::removeDatabase(qtConn);
        return {false, {}, {},
            QStringLiteral("Connection error: %1").arg(error)};
    }

    ToolExecResult result;
    {
        QSqlQuery q(QSqlDatabase::database(qtConn));
        if (!q.exec(sql)) {
            result = {false, {}, {},
                QStringLiteral("SQL error: %1").arg(q.lastError().text())};
        } else {
            const int affected = q.numRowsAffected();
            result = {true, {},
                QStringLiteral("OK. %1 row(s) affected.").arg(affected), {}};
        }
        QSqlDatabase::database(qtConn).close();
    }
    QSqlDatabase::removeDatabase(qtConn);
    return result;
}

// ── tables ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doListTables(const QString &connId)
{
    DbConnectionConfig cfg;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_configs.contains(connId))
            return {false, {}, {},
                QStringLiteral("No connection named '%1'. Use 'connect' first.").arg(connId)};
        cfg = m_configs[connId];
    }

    const QString qtConn = QStringLiteral("proj_%1_%2")
        .arg(connId.left(8),
             QUuid::createUuid().toString(QUuid::Id128).left(6));

    QString error;
    if (!openConnection(cfg, qtConn, &error)) {
        QSqlDatabase::removeDatabase(qtConn);
        return {false, {}, {},
            QStringLiteral("Connection error: %1").arg(error)};
    }

    ToolExecResult result;
    {
        const QStringList tables = QSqlDatabase::database(qtConn).tables();
        result = tables.isEmpty()
            ? ToolExecResult{true, {}, QStringLiteral("No tables in database."), {}}
            : ToolExecResult{true, {}, tables.join(QLatin1Char('\n')), {}};
        QSqlDatabase::database(qtConn).close();
    }
    QSqlDatabase::removeDatabase(qtConn);
    return result;
}

// ── schema ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doSchema(const QString &connId,
                                             const QString &table)
{
    DbConnectionConfig cfg;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_configs.contains(connId))
            return {false, {}, {},
                QStringLiteral("No connection named '%1'. Use 'connect' first.").arg(connId)};
        cfg = m_configs[connId];
    }

    const QString qtConn = QStringLiteral("proj_%1_%2")
        .arg(connId.left(8),
             QUuid::createUuid().toString(QUuid::Id128).left(6));

    QString error;
    if (!openConnection(cfg, qtConn, &error)) {
        QSqlDatabase::removeDatabase(qtConn);
        return {false, {}, {},
            QStringLiteral("Connection error: %1").arg(error)};
    }

    ToolExecResult result;
    {
        QSqlQuery q(QSqlDatabase::database(qtConn));
        // For non-SQLite: use INFORMATION_SCHEMA
        const bool isSQLite = cfg.driver == QLatin1String("QSQLITE");

        if (isSQLite) {
            if (table.isEmpty())
                q.exec(QStringLiteral(
                    "SELECT sql FROM sqlite_master WHERE type IN ('table','view') ORDER BY name"));
            else {
                q.prepare(QStringLiteral(
                    "SELECT sql FROM sqlite_master WHERE name = ? AND type IN ('table','view')"));
                q.addBindValue(table);
                q.exec();
            }
        } else {
            if (table.isEmpty()) {
                q.exec(QStringLiteral(
                    "SELECT TABLE_NAME, COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT "
                    "FROM INFORMATION_SCHEMA.COLUMNS ORDER BY TABLE_NAME, ORDINAL_POSITION"));
            } else {
                q.prepare(QStringLiteral(
                    "SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT "
                    "FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_NAME = ? "
                    "ORDER BY ORDINAL_POSITION"));
                q.addBindValue(table);
                q.exec();
            }
        }

        if (q.lastError().isValid()) {
            result = {false, {}, {},
                QStringLiteral("SQL error: %1").arg(q.lastError().text())};
        } else if (isSQLite) {
            QString schema;
            while (q.next())
                schema += q.value(0).toString() + QStringLiteral(";\n\n");
            result = schema.isEmpty()
                ? ToolExecResult{true, {},
                    table.isEmpty()
                        ? QStringLiteral("No tables.")
                        : QStringLiteral("Table '%1' not found.").arg(table),
                    {}}
                : ToolExecResult{true, {}, schema, {}};
        } else {
            result = {true, {}, formatResults(q, 500), {}};
        }
        QSqlDatabase::database(qtConn).close();
    }
    QSqlDatabase::removeDatabase(qtConn);
    return result;
}

// ── list_connections ──────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doListConnections()
{
    QMutexLocker lock(&m_mutex);
    if (m_configs.isEmpty())
        return {true, {},
            QStringLiteral("No connections. Use 'connect' to add one."), {}};

    QString result;
    for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
        const auto &c = it.value();
        result += QStringLiteral("%1: %2 @ %3")
            .arg(it.key(), c.driver,
                 c.host.isEmpty() ? c.database : c.host);
        if (c.port > 0)
            result += QStringLiteral(":%1").arg(c.port);
        if (!c.database.isEmpty() && !c.host.isEmpty())
            result += QStringLiteral("/%1").arg(c.database);
        result += QLatin1Char('\n');
    }
    return {true, {}, result.trimmed(), {}};
}

// ── list_drivers ──────────────────────────────────────────────────────────────

ToolExecResult ProjectDatabaseTool::doListDrivers()
{
    const QStringList drivers = QSqlDatabase::drivers();
    if (drivers.isEmpty())
        return {true, {}, QStringLiteral("No SQL drivers available."), {}};

    QString text = QStringLiteral("Available Qt SQL drivers:\n");
    for (const QString &d : drivers) {
        QString friendly;
        if (d == QLatin1String("QSQLITE"))       friendly = QStringLiteral("SQLite");
        else if (d == QLatin1String("QMYSQL"))    friendly = QStringLiteral("MySQL / MariaDB");
        else if (d == QLatin1String("QPSQL"))     friendly = QStringLiteral("PostgreSQL");
        else if (d == QLatin1String("QODBC"))     friendly = QStringLiteral("ODBC (SQL Server, etc.)");
        else if (d == QLatin1String("QOCI"))      friendly = QStringLiteral("Oracle");
        else if (d == QLatin1String("QIBASE"))    friendly = QStringLiteral("InterBase / Firebird");
        else if (d == QLatin1String("QMIMER"))    friendly = QStringLiteral("Mimer SQL");
        else friendly = d;

        text += QStringLiteral("  %1 — %2\n").arg(d, friendly);
    }
    return {true, {}, text.trimmed(), {}};
}

// ── openConnection ────────────────────────────────────────────────────────────

bool ProjectDatabaseTool::openConnection(const DbConnectionConfig &cfg,
                                         const QString &qtConnName,
                                         QString *errorOut)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(cfg.driver, qtConnName);
    db.setDatabaseName(cfg.database);
    if (!cfg.host.isEmpty())
        db.setHostName(cfg.host);
    if (cfg.port > 0)
        db.setPort(cfg.port);
    if (!cfg.user.isEmpty())
        db.setUserName(cfg.user);
    if (!cfg.password.isEmpty())
        db.setPassword(cfg.password);

    if (!db.open()) {
        if (errorOut)
            *errorOut = db.lastError().text();
        return false;
    }
    return true;
}

// ── resolveDriver ─────────────────────────────────────────────────────────────

QString ProjectDatabaseTool::resolveDriver(const QString &dbType)
{
    if (dbType == QLatin1String("sqlite"))
        return QStringLiteral("QSQLITE");
    if (dbType == QLatin1String("mysql") || dbType == QLatin1String("mariadb"))
        return QStringLiteral("QMYSQL");
    if (dbType == QLatin1String("postgres") || dbType == QLatin1String("postgresql"))
        return QStringLiteral("QPSQL");
    if (dbType == QLatin1String("sqlserver") || dbType == QLatin1String("mssql"))
        return QStringLiteral("QODBC");
    if (dbType == QLatin1String("oracle"))
        return QStringLiteral("QOCI");
    if (dbType == QLatin1String("odbc"))
        return QStringLiteral("QODBC");
    if (dbType == QLatin1String("firebird") || dbType == QLatin1String("interbase"))
        return QStringLiteral("QIBASE");
    return {};
}

// ── formatResults ─────────────────────────────────────────────────────────────

QString ProjectDatabaseTool::formatResults(QSqlQuery &q, int maxRows)
{
    const QSqlRecord rec = q.record();
    const int cols = rec.count();

    QStringList headers;
    for (int c = 0; c < cols; ++c)
        headers << rec.fieldName(c);

    QStringList rows;
    int rowCount = 0;
    while (q.next() && rowCount < maxRows) {
        QStringList vals;
        for (int c = 0; c < cols; ++c)
            vals << q.value(c).toString();
        rows << vals.join(QStringLiteral(" | "));
        ++rowCount;
    }

    QString result = headers.join(QStringLiteral(" | ")) + QLatin1Char('\n');
    QStringList seps;
    for (int c = 0; c < cols; ++c)
        seps << QStringLiteral("---");
    result += seps.join(QStringLiteral(" | ")) + QLatin1Char('\n');
    result += rows.join(QLatin1Char('\n'));

    if (q.next())
        result += QStringLiteral("\n... (truncated, showing first %1)").arg(maxRows);

    result += QStringLiteral("\n\n%1 row(s).").arg(rowCount);
    return result;
}
