#include "codegraphquery.h"

#include <QJsonDocument>
#include <QSqlQuery>
#include <QVariant>

CodeGraphQuery::CodeGraphQuery(QSqlDatabase &db)
    : m_db(db)  // QSqlDatabase uses implicit sharing; copy is safe and cheap
{}

// ── Helpers ──────────────────────────────────────────────────────────────────

bool CodeGraphQuery::tableExists(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
    q.bindValue(0, name);
    q.exec();
    return q.next();
}

CodeGraphQuery::FileMatch CodeGraphQuery::resolveTarget(const QString &target) const
{
    const bool isFileLike = target.contains(QLatin1Char('.')) ||
                            target.contains(QLatin1Char('/')) ||
                            target.contains(QLatin1Char('\\'));

    if (isFileLike) {
        // Try exact path, then fuzzy
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, path FROM files WHERE path = ? OR path LIKE ? ESCAPE '\\'"));
        q.bindValue(0, target);
        QString escaped = target;
        escaped.replace(QLatin1Char('%'), QLatin1String("\\%"));
        escaped.replace(QLatin1Char('_'), QLatin1String("\\_"));
        q.bindValue(1, QLatin1Char('%') + escaped + QLatin1Char('%'));
        q.exec();
        if (q.next())
            return {q.value(0).toLongLong(), q.value(1).toString()};
    } else {
        // Try class name first for bare names
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT f.id, f.path FROM classes c "
            "JOIN files f ON c.file_id = f.id WHERE c.name = ?"));
        q.bindValue(0, target);
        q.exec();
        if (q.next())
            return {q.value(0).toLongLong(), q.value(1).toString()};

        // Fall back to fuzzy file path (.cpp/.h only)
        q.prepare(QStringLiteral(
            "SELECT id, path FROM files "
            "WHERE path LIKE ? AND ext IN ('.cpp', '.h', '.py')"));
        q.bindValue(0, QLatin1Char('%') + target + QLatin1Char('%'));
        q.exec();
        if (q.next())
            return {q.value(0).toLongLong(), q.value(1).toString()};
    }

    return {};
}

// ── context ──────────────────────────────────────────────────────────────────

QJsonObject CodeGraphQuery::context(const QString &target) const
{
    auto match = resolveTarget(target);
    if (match.id == 0) return {};

    QJsonObject result;
    result[QStringLiteral("path")] = match.path;

    // File info
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT lang, lines, subsystem FROM files WHERE id = ?"));
        q.bindValue(0, match.id);
        q.exec();
        if (q.next()) {
            result[QStringLiteral("lang")]      = q.value(0).toString();
            result[QStringLiteral("lines")]     = q.value(1).toInt();
            result[QStringLiteral("subsystem")] = q.value(2).toString();
        }
    }

    // Summary
    if (tableExists(QStringLiteral("file_summaries"))) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT summary, category, key_classes "
            "FROM file_summaries WHERE file_id = ?"));
        q.bindValue(0, match.id);
        q.exec();
        if (q.next()) {
            result[QStringLiteral("summary")]     = q.value(0).toString();
            result[QStringLiteral("category")]    = q.value(1).toString();
            result[QStringLiteral("key_classes")] = q.value(2).toString();
        }
    }

    // Dependencies (outgoing includes)
    if (tableExists(QStringLiteral("edges"))) {
        QJsonArray deps;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT f.path FROM edges e "
            "JOIN files f ON e.target_file = f.id "
            "WHERE e.source_file = ? AND e.edge_type = 'includes' "
            "ORDER BY f.path"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next())
            deps.append(q.value(0).toString());
        if (!deps.isEmpty())
            result[QStringLiteral("dependencies")] = deps;
    }

    // Reverse dependencies
    if (tableExists(QStringLiteral("edges"))) {
        QJsonArray rdeps;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT f.path FROM edges e "
            "JOIN files f ON e.source_file = f.id "
            "WHERE e.target_file = ? AND e.edge_type = 'includes' "
            "ORDER BY f.path"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next())
            rdeps.append(q.value(0).toString());
        if (!rdeps.isEmpty())
            result[QStringLiteral("reverse_dependencies")] = rdeps;
    }

    // Test files
    if (tableExists(QStringLiteral("edges"))) {
        QJsonArray tests;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT f.path FROM edges e "
            "JOIN files f ON e.source_file = f.id "
            "WHERE e.target_file = ? AND e.edge_type = 'tests'"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next())
            tests.append(q.value(0).toString());
        if (!tests.isEmpty())
            result[QStringLiteral("tests")] = tests;
    }

    // Functions with line ranges
    if (tableExists(QStringLiteral("function_index"))) {
        QJsonArray funcs;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT name, qualified_name, start_line, end_line, line_count "
            "FROM function_index WHERE file_id = ? ORDER BY start_line"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject fn;
            fn[QStringLiteral("name")]          = q.value(0).toString();
            fn[QStringLiteral("qualified_name")] = q.value(1).toString();
            fn[QStringLiteral("start_line")]    = q.value(2).toInt();
            fn[QStringLiteral("end_line")]      = q.value(3).toInt();
            fn[QStringLiteral("lines")]         = q.value(4).toInt();
            funcs.append(fn);
        }
        if (!funcs.isEmpty())
            result[QStringLiteral("functions")] = funcs;
    }

    // Qt connections
    if (tableExists(QStringLiteral("qt_connections"))) {
        QJsonArray conns;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT sender, signal_name, receiver, slot_name, line_num "
            "FROM qt_connections WHERE file_id = ?"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject c;
            c[QStringLiteral("sender")]   = q.value(0).toString();
            c[QStringLiteral("signal")]   = q.value(1).toString();
            c[QStringLiteral("receiver")] = q.value(2).toString();
            c[QStringLiteral("slot")]     = q.value(3).toString();
            c[QStringLiteral("line")]     = q.value(4).toInt();
            conns.append(c);
        }
        if (!conns.isEmpty())
            result[QStringLiteral("connections")] = conns;
    }

    // Services
    if (tableExists(QStringLiteral("services"))) {
        QJsonArray svcs;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT service_key, reg_type, line_num "
            "FROM services WHERE file_id = ?"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject s;
            s[QStringLiteral("key")]  = q.value(0).toString();
            s[QStringLiteral("type")] = q.value(1).toString();
            s[QStringLiteral("line")] = q.value(2).toInt();
            svcs.append(s);
        }
        if (!svcs.isEmpty())
            result[QStringLiteral("services")] = svcs;
    }

    return result;
}

// ── functions ────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::functions(const QString &target) const
{
    if (!tableExists(QStringLiteral("function_index")))
        return {};

    auto match = resolveTarget(target);
    if (match.id == 0) return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT name, qualified_name, start_line, end_line, line_count, "
        "is_method, class_name FROM function_index "
        "WHERE file_id = ? ORDER BY start_line"));
    q.bindValue(0, match.id);
    q.exec();

    while (q.next()) {
        QJsonObject fn;
        fn[QStringLiteral("name")]           = q.value(0).toString();
        fn[QStringLiteral("qualified_name")] = q.value(1).toString();
        fn[QStringLiteral("start_line")]     = q.value(2).toInt();
        fn[QStringLiteral("end_line")]       = q.value(3).toInt();
        fn[QStringLiteral("lines")]          = q.value(4).toInt();
        fn[QStringLiteral("is_method")]      = q.value(5).toBool();
        fn[QStringLiteral("class_name")]     = q.value(6).toString();
        arr.append(fn);
    }
    return arr;
}

// ── edges ────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::edges(const QString &target) const
{
    if (!tableExists(QStringLiteral("edges")))
        return {};

    auto match = resolveTarget(target);
    if (match.id == 0) return {};

    QJsonArray arr;

    // Outgoing
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT e.edge_type, e.target_name, f.path "
            "FROM edges e LEFT JOIN files f ON e.target_file = f.id "
            "WHERE e.source_file = ? ORDER BY e.edge_type"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject e;
            e[QStringLiteral("direction")] = QStringLiteral("outgoing");
            e[QStringLiteral("type")]      = q.value(0).toString();
            e[QStringLiteral("name")]      = q.value(1).toString();
            e[QStringLiteral("path")]      = q.value(2).toString();
            arr.append(e);
        }
    }

    // Incoming
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT e.edge_type, e.source_name, f.path "
            "FROM edges e LEFT JOIN files f ON e.source_file = f.id "
            "WHERE e.target_file = ? ORDER BY e.edge_type"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject e;
            e[QStringLiteral("direction")] = QStringLiteral("incoming");
            e[QStringLiteral("type")]      = q.value(0).toString();
            e[QStringLiteral("name")]      = q.value(1).toString();
            e[QStringLiteral("path")]      = q.value(2).toString();
            arr.append(e);
        }
    }

    return arr;
}

// ── search (FTS5) ────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::search(const QString &query, int limit) const
{
    if (!tableExists(QStringLiteral("fts_index")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);

    // Quote the query for FTS5 safety
    QString safeQuery = query;
    safeQuery.replace(QLatin1Char('"'), QLatin1String("\"\""));

    q.prepare(QStringLiteral(
        "SELECT name, qualified_name, file_path, kind, summary "
        "FROM fts_index WHERE fts_index MATCH ? ORDER BY rank LIMIT ?"));
    q.bindValue(0, QLatin1Char('"') + safeQuery + QLatin1Char('"'));
    q.bindValue(1, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("name")]           = q.value(0).toString();
        item[QStringLiteral("qualified_name")] = q.value(1).toString();
        item[QStringLiteral("file_path")]      = q.value(2).toString();
        item[QStringLiteral("kind")]           = q.value(3).toString();
        item[QStringLiteral("summary")]        = q.value(4).toString();
        arr.append(item);
    }
    return arr;
}

// ── projectSummary ───────────────────────────────────────────────────────────

QJsonObject CodeGraphQuery::projectSummary() const
{
    QJsonObject result;
    QSqlQuery q(m_db);

    static const QSet<QString> validTables = {
        QStringLiteral("files"),         QStringLiteral("classes"),
        QStringLiteral("methods"),       QStringLiteral("includes"),
        QStringLiteral("function_index"),QStringLiteral("edges"),
        QStringLiteral("qt_connections"),QStringLiteral("services"),
        QStringLiteral("test_cases"),    QStringLiteral("cmake_targets"),
        QStringLiteral("subsystems"),
    };

    auto countTable = [&](const QString &table) -> int {
        if (!validTables.contains(table)) return 0;
        if (!tableExists(table)) return 0;
        q.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table);
        return q.next() ? q.value(0).toInt() : 0;
    };

    result[QStringLiteral("files")]       = countTable(QStringLiteral("files"));
    result[QStringLiteral("classes")]     = countTable(QStringLiteral("classes"));
    result[QStringLiteral("methods")]     = countTable(QStringLiteral("methods"));
    result[QStringLiteral("includes")]    = countTable(QStringLiteral("includes"));
    result[QStringLiteral("functions")]   = countTable(QStringLiteral("function_index"));
    result[QStringLiteral("edges")]       = countTable(QStringLiteral("edges"));
    result[QStringLiteral("connections")] = countTable(QStringLiteral("qt_connections"));
    result[QStringLiteral("services")]    = countTable(QStringLiteral("services"));
    result[QStringLiteral("test_cases")]  = countTable(QStringLiteral("test_cases"));
    result[QStringLiteral("cmake_targets")] = countTable(QStringLiteral("cmake_targets"));
    result[QStringLiteral("subsystems")]  = countTable(QStringLiteral("subsystems"));

    return result;
}

// ── connections ──────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::connections(const QString &target) const
{
    if (!tableExists(QStringLiteral("qt_connections")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);

    if (target.isEmpty()) {
        q.exec(QStringLiteral(
            "SELECT qc.sender, qc.signal_name, qc.receiver, qc.slot_name, "
            "qc.line_num, f.path FROM qt_connections qc "
            "JOIN files f ON qc.file_id = f.id ORDER BY f.path"));
    } else {
        auto match = resolveTarget(target);
        if (match.id == 0) return {};
        q.prepare(QStringLiteral(
            "SELECT qc.sender, qc.signal_name, qc.receiver, qc.slot_name, "
            "qc.line_num, f.path FROM qt_connections qc "
            "JOIN files f ON qc.file_id = f.id WHERE qc.file_id = ?"));
        q.bindValue(0, match.id);
        q.exec();
    }

    while (q.next()) {
        QJsonObject c;
        c[QStringLiteral("sender")]   = q.value(0).toString();
        c[QStringLiteral("signal")]   = q.value(1).toString();
        c[QStringLiteral("receiver")] = q.value(2).toString();
        c[QStringLiteral("slot")]     = q.value(3).toString();
        c[QStringLiteral("line")]     = q.value(4).toInt();
        c[QStringLiteral("file")]     = q.value(5).toString();
        arr.append(c);
    }
    return arr;
}

// ── services ─────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::services() const
{
    if (!tableExists(QStringLiteral("services")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT s.service_key, s.reg_type, s.line_num, f.path "
        "FROM services s JOIN files f ON s.file_id = f.id "
        "ORDER BY s.service_key"));

    while (q.next()) {
        QJsonObject s;
        s[QStringLiteral("key")]  = q.value(0).toString();
        s[QStringLiteral("type")] = q.value(1).toString();
        s[QStringLiteral("line")] = q.value(2).toInt();
        s[QStringLiteral("file")] = q.value(3).toString();
        arr.append(s);
    }
    return arr;
}

// ── testCases ────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::testCases(const QString &target) const
{
    if (!tableExists(QStringLiteral("test_cases")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);

    if (target.isEmpty()) {
        q.exec(QStringLiteral(
            "SELECT tc.test_class, tc.test_method, tc.line_num, f.path "
            "FROM test_cases tc JOIN files f ON tc.file_id = f.id "
            "ORDER BY tc.test_class, tc.test_method"));
    } else {
        auto match = resolveTarget(target);
        if (match.id == 0) return {};
        q.prepare(QStringLiteral(
            "SELECT tc.test_class, tc.test_method, tc.line_num, f.path "
            "FROM test_cases tc JOIN files f ON tc.file_id = f.id "
            "WHERE tc.file_id = ?"));
        q.bindValue(0, match.id);
        q.exec();
    }

    while (q.next()) {
        QJsonObject t;
        t[QStringLiteral("class")]  = q.value(0).toString();
        t[QStringLiteral("method")] = q.value(1).toString();
        t[QStringLiteral("line")]   = q.value(2).toInt();
        t[QStringLiteral("file")]   = q.value(3).toString();
        arr.append(t);
    }
    return arr;
}

// ── cmakeTargets ─────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::cmakeTargets() const
{
    if (!tableExists(QStringLiteral("cmake_targets")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT ct.target_name, ct.target_type, ct.line_num, "
        "COALESCE(f.path, '(CMakeLists.txt)') "
        "FROM cmake_targets ct "
        "LEFT JOIN files f ON ct.file_id = f.id "
        "ORDER BY ct.target_type, ct.target_name"));

    while (q.next()) {
        QJsonObject t;
        t[QStringLiteral("name")] = q.value(0).toString();
        t[QStringLiteral("type")] = q.value(1).toString();
        t[QStringLiteral("line")] = q.value(2).toInt();
        t[QStringLiteral("file")] = q.value(3).toString();
        arr.append(t);
    }
    return arr;
}

// ── subsystems ───────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::subsystems() const
{
    if (!tableExists(QStringLiteral("subsystems")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT name, dir_path, file_count, line_count, class_count, "
        "h_only_count, interface_count FROM subsystems "
        "ORDER BY line_count DESC"));

    while (q.next()) {
        QJsonObject s;
        s[QStringLiteral("name")]        = q.value(0).toString();
        s[QStringLiteral("dir")]         = q.value(1).toString();
        s[QStringLiteral("files")]       = q.value(2).toInt();
        s[QStringLiteral("lines")]       = q.value(3).toInt();
        s[QStringLiteral("classes")]     = q.value(4).toInt();
        s[QStringLiteral("header_only")] = q.value(5).toInt();
        s[QStringLiteral("interfaces")]  = q.value(6).toInt();
        arr.append(s);
    }
    return arr;
}
