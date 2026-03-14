#include "agentmemorydb.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

// ── helpers ───────────────────────────────────────────────────────────────────

QString AgentMemoryDbTool::storageDir()
{
    const QString dir = QCoreApplication::applicationDirPath()
                        + QStringLiteral("/agentmemory");
    QDir().mkpath(dir);
    return dir;
}

QString AgentMemoryDbTool::sanitizeName(const QString &name)
{
    QString safe;
    safe.reserve(name.size());
    for (const QChar &c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-'))
            safe += c;
    }
    return safe.left(64);
}

QString AgentMemoryDbTool::dbPath(const QString &safeName)
{
    return storageDir() + QLatin1Char('/') + safeName + QStringLiteral(".db");
}

QString AgentMemoryDbTool::metaPath(const QString &safeName)
{
    return storageDir() + QLatin1Char('/') + safeName + QStringLiteral(".json");
}

// ── spec ──────────────────────────────────────────────────────────────────────

ToolSpec AgentMemoryDbTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("agent_memory_db");
    s.description = QStringLiteral(
        "Your private SQLite database manager. Create, delete, and query "
        "multiple named databases for structured long-term memory.\n"
        "Databases are stored in <app_dir>/agentmemory/ and persist across sessions.\n\n"
        "Operations:\n"
        "  'create_database' — create a new named database\n"
        "  'delete_database' — delete a database and its metadata\n"
        "  'list_databases'  — list all databases with metadata\n"
        "  'info'            — show database details (tables, size)\n"
        "  'query'           — run SELECT on a named database\n"
        "  'execute'         — run DDL/DML (CREATE TABLE, INSERT, etc.)\n"
        "  'tables'          — list tables in a named database\n"
        "  'schema'          — show CREATE statements for tables");
    s.permission   = AgentToolPermission::SafeMutate;
    s.timeoutMs    = 15000;
    s.parallelSafe = true;
    s.inputSchema  = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: create_database, delete_database, "
                    "list_databases, info, query, execute, tables, schema")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("create_database"), QStringLiteral("delete_database"),
                    QStringLiteral("list_databases"), QStringLiteral("info"),
                    QStringLiteral("query"), QStringLiteral("execute"),
                    QStringLiteral("tables"), QStringLiteral("schema")}}
            }},
            {QStringLiteral("name"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Database name (alphanumeric, underscores, hyphens, max 64 chars).")}
            }},
            {QStringLiteral("description"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Description for the database (used with create_database).")}
            }},
            {QStringLiteral("sql"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("SQL statement (for query/execute).")}
            }},
            {QStringLiteral("table"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Table name (for schema, optional).")}
            }},
            {QStringLiteral("maxRows"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Max rows to return (default: 200, max: 2000).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

// ── invoke ────────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::invoke(const QJsonObject &args)
{
    const QString op   = args[QLatin1String("operation")].toString();
    const QString name = args[QLatin1String("name")].toString();

    if (op == QLatin1String("list_databases"))
        return doListDatabases();
    if (op == QLatin1String("create_database"))
        return doCreateDatabase(name, args[QLatin1String("description")].toString());
    if (op == QLatin1String("delete_database"))
        return doDeleteDatabase(name);

    // All remaining ops require a database name
    if (name.isEmpty())
        return {false, {}, {},
            QStringLiteral("'name' is required for '%1'.").arg(op)};

    if (op == QLatin1String("info"))
        return doDatabaseInfo(name);
    if (op == QLatin1String("query")) {
        const int maxRows = qBound(1, args[QLatin1String("maxRows")].toInt(200), 2000);
        return doQuery(name, args[QLatin1String("sql")].toString(), maxRows);
    }
    if (op == QLatin1String("execute"))
        return doExecute(name, args[QLatin1String("sql")].toString());
    if (op == QLatin1String("tables"))
        return doListTables(name);
    if (op == QLatin1String("schema"))
        return doSchema(name, args[QLatin1String("table")].toString());

    return {false, {}, {},
        QStringLiteral("Unknown operation: '%1'.").arg(op)};
}

// ── create_database ───────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doCreateDatabase(const QString &name,
                                                   const QString &description)
{
    if (name.isEmpty())
        return {false, {}, {},
            QStringLiteral("'name' is required for 'create_database'.")};

    const QString safe = sanitizeName(name);
    if (safe.isEmpty())
        return {false, {}, {},
            QStringLiteral("Invalid name. Use alphanumeric, underscores, hyphens.")};

    const QString db  = dbPath(safe);
    const QString meta = metaPath(safe);

    if (QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' already exists.").arg(safe)};

    // Create the SQLite database by opening & closing
    const QString connName = QStringLiteral("mem_create_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (!sqlDb.open()) {
            const QString err = sqlDb.lastError().text();
            QSqlDatabase::removeDatabase(connName);
            return {false, {}, {},
                QStringLiteral("Failed to create database: %1").arg(err)};
        }
        sqlDb.close();
    }
    QSqlDatabase::removeDatabase(connName);

    // Write metadata sidecar
    QJsonObject metaObj;
    metaObj[QStringLiteral("name")]        = safe;
    metaObj[QStringLiteral("description")] = description;
    metaObj[QStringLiteral("created")]     = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile metaFile(meta);
    if (metaFile.open(QIODevice::WriteOnly | QIODevice::Text))
        metaFile.write(QJsonDocument(metaObj).toJson(QJsonDocument::Compact));

    return {true, {},
        QStringLiteral("Database '%1' created at %2").arg(safe, db), {}};
}

// ── delete_database ───────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doDeleteDatabase(const QString &name)
{
    if (name.isEmpty())
        return {false, {}, {},
            QStringLiteral("'name' is required for 'delete_database'.")};

    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);
    const QString meta = metaPath(safe);

    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    QFile::remove(db);
    QFile::remove(meta);    // metadata may or may not exist

    return {true, {},
        QStringLiteral("Database '%1' deleted.").arg(safe), {}};
}

// ── list_databases ────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doListDatabases()
{
    QDir dir(storageDir());
    const QFileInfoList dbFiles = dir.entryInfoList(
        {QStringLiteral("*.db")}, QDir::Files, QDir::Name);

    if (dbFiles.isEmpty())
        return {true, {},
            QStringLiteral("No databases. Use 'create_database' to create one."), {}};

    QString result;
    for (const QFileInfo &fi : dbFiles) {
        const QString baseName = fi.completeBaseName();
        const QString meta     = metaPath(baseName);

        result += QStringLiteral("**%1**").arg(baseName);

        // Read metadata sidecar
        QFile metaFile(meta);
        if (metaFile.open(QIODevice::ReadOnly)) {
            const QJsonObject obj = QJsonDocument::fromJson(metaFile.readAll()).object();
            const QString desc    = obj[QStringLiteral("description")].toString();
            const QString created = obj[QStringLiteral("created")].toString();
            if (!desc.isEmpty())
                result += QStringLiteral(" — %1").arg(desc);
            if (!created.isEmpty())
                result += QStringLiteral("  (created: %1)").arg(created);
        }

        result += QStringLiteral("  [%1 KB]\n").arg(fi.size() / 1024);
    }

    return {true, {}, result.trimmed(), {}};
}

// ── info ──────────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doDatabaseInfo(const QString &name)
{
    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);

    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    const QFileInfo fi(db);
    QString result = QStringLiteral("Database: %1\nFile: %2\nSize: %3 KB\n")
        .arg(safe, db).arg(fi.size() / 1024);

    // Read metadata
    QFile metaFile(metaPath(safe));
    if (metaFile.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(metaFile.readAll()).object();
        const QString desc    = obj[QStringLiteral("description")].toString();
        const QString created = obj[QStringLiteral("created")].toString();
        if (!desc.isEmpty())
            result += QStringLiteral("Description: %1\n").arg(desc);
        if (!created.isEmpty())
            result += QStringLiteral("Created: %1\n").arg(created);
    }

    // List tables
    const QString connName = QStringLiteral("mem_info_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (sqlDb.open()) {
            const QStringList tables = sqlDb.tables();
            result += QStringLiteral("Tables (%1): %2\n")
                .arg(tables.size())
                .arg(tables.isEmpty()
                     ? QStringLiteral("(none)")
                     : tables.join(QStringLiteral(", ")));
            sqlDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);

    return {true, {}, result.trimmed(), {}};
}

// ── query ─────────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doQuery(const QString &name,
                                          const QString &sql, int maxRows)
{
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'query'.")};

    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);
    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    const QString connName = QStringLiteral("mem_q_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (!sqlDb.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open '%1': %2").arg(safe, sqlDb.lastError().text())};
        } else {
            QSqlQuery q(sqlDb);
            if (!q.exec(sql))
                result = {false, {}, {},
                    QStringLiteral("SQL error: %1").arg(q.lastError().text())};
            else
                result = {true, {}, formatResults(q, maxRows), {}};
            sqlDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── execute ───────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doExecute(const QString &name,
                                            const QString &sql)
{
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'execute'.")};

    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);
    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    const QString connName = QStringLiteral("mem_x_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (!sqlDb.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open '%1': %2").arg(safe, sqlDb.lastError().text())};
        } else {
            QSqlQuery q(sqlDb);
            if (!q.exec(sql))
                result = {false, {}, {},
                    QStringLiteral("SQL error: %1").arg(q.lastError().text())};
            else
                result = {true, {},
                    QStringLiteral("OK. %1 row(s) affected.").arg(q.numRowsAffected()), {}};
            sqlDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── tables ────────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doListTables(const QString &name)
{
    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);
    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    const QString connName = QStringLiteral("mem_t_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (!sqlDb.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open '%1': %2").arg(safe, sqlDb.lastError().text())};
        } else {
            const QStringList tables = sqlDb.tables();
            result = tables.isEmpty()
                ? ToolExecResult{true, {},
                    QStringLiteral("No tables in '%1'. Use 'execute' with CREATE TABLE.").arg(safe), {}}
                : ToolExecResult{true, {}, tables.join(QLatin1Char('\n')), {}};
            sqlDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── schema ────────────────────────────────────────────────────────────────────

ToolExecResult AgentMemoryDbTool::doSchema(const QString &name,
                                           const QString &table)
{
    const QString safe = sanitizeName(name);
    const QString db   = dbPath(safe);
    if (!QFileInfo::exists(db))
        return {false, {}, {},
            QStringLiteral("Database '%1' not found.").arg(safe)};

    const QString connName = QStringLiteral("mem_s_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase sqlDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqlDb.setDatabaseName(db);
        if (!sqlDb.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open '%1': %2").arg(safe, sqlDb.lastError().text())};
        } else {
            QSqlQuery q(sqlDb);
            if (table.isEmpty()) {
                q.exec(QStringLiteral(
                    "SELECT sql FROM sqlite_master WHERE type IN ('table','view') ORDER BY name"));
            } else {
                q.prepare(QStringLiteral(
                    "SELECT sql FROM sqlite_master WHERE name = ? AND type IN ('table','view')"));
                q.addBindValue(table);
                q.exec();
            }

            if (q.lastError().isValid()) {
                result = {false, {}, {},
                    QStringLiteral("SQL error: %1").arg(q.lastError().text())};
            } else {
                QString schema;
                while (q.next())
                    schema += q.value(0).toString() + QStringLiteral(";\n\n");
                result = schema.isEmpty()
                    ? ToolExecResult{true, {},
                        table.isEmpty()
                            ? QStringLiteral("No tables in '%1'.").arg(safe)
                            : QStringLiteral("Table '%1' not found in '%2'.").arg(table, safe),
                        {}}
                    : ToolExecResult{true, {}, schema, {}};
            }
            sqlDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── formatResults ─────────────────────────────────────────────────────────────

QString AgentMemoryDbTool::formatResults(QSqlQuery &q, int maxRows)
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
