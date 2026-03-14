#include "databasetool.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

// ── DatabaseTool ─────────────────────────────────────────────────────────────

ToolSpec DatabaseTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("database_query");
    s.description = QStringLiteral(
        "Execute SQL queries against SQLite database files in the workspace. "
        "Operations:\n"
        "  'query' — execute a SELECT query and return results\n"
        "  'execute' — execute INSERT/UPDATE/DELETE/CREATE statements\n"
        "  'schema' — show database schema (tables, columns)\n"
        "  'tables' — list all tables in the database\n\n"
        "Only opens .db, .sqlite, .sqlite3 files within the workspace.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: query, execute, schema, tables")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("query"), QStringLiteral("execute"),
                    QStringLiteral("schema"), QStringLiteral("tables")}}
            }},
            {QStringLiteral("dbPath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the SQLite database file.")}
            }},
            {QStringLiteral("sql"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("SQL query or statement to execute.")}
            }},
            {QStringLiteral("maxRows"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum rows to return (default: 100, max: 1000).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("operation"), QStringLiteral("dbPath")}}
    };
    return s;
}

ToolExecResult DatabaseTool::invoke(const QJsonObject &args)
{
    const QString op      = args[QLatin1String("operation")].toString();
    const QString rawPath = args[QLatin1String("dbPath")].toString();

    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("'dbPath' is required.")};

    // Resolve and validate path
    QString dbPath = rawPath;
    if (!QFileInfo(dbPath).isAbsolute() && !m_workspaceRoot.isEmpty())
        dbPath = QDir(m_workspaceRoot).absoluteFilePath(dbPath);

    // Security: only allow .db, .sqlite, .sqlite3 extensions
    const QString ext = QFileInfo(dbPath).suffix().toLower();
    if (ext != QLatin1String("db") && ext != QLatin1String("sqlite")
        && ext != QLatin1String("sqlite3"))
        return {false, {}, {},
            QStringLiteral("Only .db, .sqlite, .sqlite3 files are allowed.")};

    // Security: must be within workspace
    if (!m_workspaceRoot.isEmpty()) {
        const QString canonical = QFileInfo(dbPath).canonicalFilePath();
        const QString wsCanonical = QFileInfo(m_workspaceRoot).canonicalFilePath();
        if (!canonical.isEmpty() && !canonical.startsWith(wsCanonical))
            return {false, {}, {},
                QStringLiteral("Database must be within workspace.")};
    }

    // Open database with unique connection name
    const QString connName = QStringLiteral("agent_db_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else if (op == QLatin1String("tables")) {
            result = listTables(db);
        } else if (op == QLatin1String("schema")) {
            result = showSchema(db);
        } else if (op == QLatin1String("query")) {
            result = executeQuery(db, args);
        } else if (op == QLatin1String("execute")) {
            result = executeStatement(db, args);
        } else {
            result = {false, {}, {},
                QStringLiteral("Unknown operation: %1").arg(op)};
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

ToolExecResult DatabaseTool::listTables(const QSqlDatabase &db)
{
    const QStringList tables = db.tables();
    if (tables.isEmpty())
        return {true, {}, QStringLiteral("No tables in database."), {}};
    return {true, {}, tables.join(QLatin1Char('\n')), {}};
}

ToolExecResult DatabaseTool::showSchema(const QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type IN ('table','view') "
        "ORDER BY name"));
    QString result;
    while (q.next())
        result += q.value(0).toString() + QStringLiteral(";\n\n");
    if (result.isEmpty())
        return {true, {}, QStringLiteral("Empty database."), {}};
    return {true, {}, result, {}};
}

ToolExecResult DatabaseTool::executeQuery(const QSqlDatabase &db,
                                          const QJsonObject &args)
{
    const QString sql = args[QLatin1String("sql")].toString();
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'query'.")};

    // Security: block write operations in query mode
    const QString upper = sql.trimmed().toUpper();
    if (upper.startsWith(QLatin1String("INSERT"))
        || upper.startsWith(QLatin1String("UPDATE"))
        || upper.startsWith(QLatin1String("DELETE"))
        || upper.startsWith(QLatin1String("DROP"))
        || upper.startsWith(QLatin1String("ALTER"))
        || upper.startsWith(QLatin1String("CREATE")))
        return {false, {}, {},
            QStringLiteral("Write operations not allowed in 'query' mode. "
                           "Use 'execute' instead.")};

    const int maxRows = qBound(1, args[QLatin1String("maxRows")].toInt(100), 1000);

    QSqlQuery q(db);
    if (!q.exec(sql))
        return {false, {}, {},
            QStringLiteral("SQL error: %1").arg(q.lastError().text())};

    return formatQueryResults(q, maxRows);
}

ToolExecResult DatabaseTool::executeStatement(const QSqlDatabase &db,
                                              const QJsonObject &args)
{
    const QString sql = args[QLatin1String("sql")].toString();
    if (sql.isEmpty())
        return {false, {}, {}, QStringLiteral("'sql' is required for 'execute'.")};

    QSqlQuery q(db);
    if (!q.exec(sql))
        return {false, {}, {},
            QStringLiteral("SQL error: %1").arg(q.lastError().text())};

    const int affected = q.numRowsAffected();
    return {true, {},
        QStringLiteral("Statement executed. %1 row(s) affected.").arg(affected), {}};
}

ToolExecResult DatabaseTool::formatQueryResults(QSqlQuery &q, int maxRows)
{
    const QSqlRecord rec = q.record();
    const int cols = rec.count();

    // Header
    QStringList headers;
    for (int c = 0; c < cols; ++c)
        headers << rec.fieldName(c);

    // Rows
    QStringList rows;
    int rowCount = 0;
    while (q.next() && rowCount < maxRows) {
        QStringList vals;
        for (int c = 0; c < cols; ++c)
            vals << q.value(c).toString();
        rows << vals.join(QStringLiteral(" | "));
        ++rowCount;
    }

    // Format as table
    QString result = headers.join(QStringLiteral(" | ")) + QLatin1Char('\n');
    // Separator
    QStringList seps;
    for (int c = 0; c < cols; ++c)
        seps << QStringLiteral("---");
    result += seps.join(QStringLiteral(" | ")) + QLatin1Char('\n');
    result += rows.join(QLatin1Char('\n'));

    if (q.next())
        result += QStringLiteral("\n... (more rows, showing first %1)").arg(maxRows);

    result += QStringLiteral("\n\n%1 row(s) returned.").arg(rowCount);
    return {true, {}, result, {}};
}
