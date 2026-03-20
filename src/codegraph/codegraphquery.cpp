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

    if (tableExists(QStringLiteral("semantic_tags"))) {
        QJsonArray tags;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT entity_type, entity_name, line_num, tag, tag_value, confidence "
            "FROM semantic_tags WHERE file_id = ? ORDER BY entity_type, tag, tag_value"));
        q.bindValue(0, match.id);
        q.exec();
        while (q.next()) {
            QJsonObject item;
            item[QStringLiteral("entity_type")] = q.value(0).toString();
            item[QStringLiteral("entity_name")] = q.value(1).toString();
            item[QStringLiteral("line")] = q.value(2).toInt();
            item[QStringLiteral("tag")] = q.value(3).toString();
            item[QStringLiteral("value")] = q.value(4).toString();
            item[QStringLiteral("confidence")] = q.value(5).toInt();
            tags.append(item);
        }
        if (!tags.isEmpty())
            result[QStringLiteral("semantic_tags")] = tags;
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
        QStringLiteral("subsystems"),    QStringLiteral("semantic_tags"),
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
    result[QStringLiteral("semantic_tags")] = countTable(QStringLiteral("semantic_tags"));
    result[QStringLiteral("call_graph_edges")] = tableExists(QStringLiteral("call_graph"))
        ? (q.exec(QStringLiteral("SELECT COUNT(*) FROM call_graph")), q.next() ? q.value(0).toInt() : 0)
        : 0;
    result[QStringLiteral("reachable")] = tableExists(QStringLiteral("reachability"))
        ? (q.exec(QStringLiteral("SELECT COUNT(*) FROM reachability WHERE is_reachable=1")), q.next() ? q.value(0).toInt() : 0)
        : 0;
    result[QStringLiteral("dead_code")] = tableExists(QStringLiteral("reachability"))
        ? (q.exec(QStringLiteral("SELECT COUNT(*) FROM reachability WHERE is_reachable=0")), q.next() ? q.value(0).toInt() : 0)
        : 0;
    result[QStringLiteral("typos")] = tableExists(QStringLiteral("symbol_aliases"))
        ? (q.exec(QStringLiteral("SELECT COUNT(*) FROM symbol_aliases WHERE alias_type='typo'")), q.next() ? q.value(0).toInt() : 0)
        : 0;
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

QJsonArray CodeGraphQuery::semanticTags(const QString &target) const
{
    if (!tableExists(QStringLiteral("semantic_tags")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);

    if (target.isEmpty()) {
        q.exec(QStringLiteral(
            "SELECT st.entity_type, st.entity_name, st.line_num, st.tag, st.tag_value, st.confidence, f.path "
            "FROM semantic_tags st JOIN files f ON st.file_id = f.id "
            "ORDER BY f.path, st.entity_type, st.tag, st.tag_value"));
    } else {
        auto match = resolveTarget(target);
        if (match.id == 0)
            return {};
        q.prepare(QStringLiteral(
            "SELECT st.entity_type, st.entity_name, st.line_num, st.tag, st.tag_value, st.confidence, f.path "
            "FROM semantic_tags st JOIN files f ON st.file_id = f.id "
            "WHERE st.file_id = ? ORDER BY st.entity_type, st.tag, st.tag_value"));
        q.bindValue(0, match.id);
        q.exec();
    }

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("entity_type")] = q.value(0).toString();
        item[QStringLiteral("entity_name")] = q.value(1).toString();
        item[QStringLiteral("line")] = q.value(2).toInt();
        item[QStringLiteral("tag")] = q.value(3).toString();
        item[QStringLiteral("value")] = q.value(4).toString();
        item[QStringLiteral("confidence")] = q.value(5).toInt();
        item[QStringLiteral("file")] = q.value(6).toString();
        arr.append(item);
    }
    return arr;
}

QJsonArray CodeGraphQuery::searchByTag(const QString &tag, int limit) const
{
    if (!tableExists(QStringLiteral("semantic_tags")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT st.entity_type, st.entity_name, st.line_num, st.tag, st.tag_value, st.confidence, f.path "
        "FROM semantic_tags st JOIN files f ON st.file_id = f.id "
        "WHERE st.tag = ? OR st.tag_value = ? "
        "ORDER BY st.confidence DESC, f.path LIMIT ?"));
    q.bindValue(0, tag);
    q.bindValue(1, tag);
    q.bindValue(2, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("entity_type")] = q.value(0).toString();
        item[QStringLiteral("entity_name")] = q.value(1).toString();
        item[QStringLiteral("line")] = q.value(2).toInt();
        item[QStringLiteral("tag")] = q.value(3).toString();
        item[QStringLiteral("value")] = q.value(4).toString();
        item[QStringLiteral("confidence")] = q.value(5).toInt();
        item[QStringLiteral("file")] = q.value(6).toString();
        arr.append(item);
    }
    return arr;
}

// ── callees ────────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::callees(const QString &funcName, int limit) const
{
    if (!tableExists(QStringLiteral("call_graph")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT cg.callee_func, COALESCE(f.path, '') "
        "FROM call_graph cg "
        "LEFT JOIN files f ON cg.callee_file_id = f.id "
        "WHERE cg.caller_func = ? "
        "ORDER BY cg.callee_func LIMIT ?"));
    q.bindValue(0, funcName);
    q.bindValue(1, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("callee")] = q.value(0).toString();
        item[QStringLiteral("file")]   = q.value(1).toString();
        arr.append(item);
    }
    return arr;
}

// ── callers ────────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::callers(const QString &funcName, int limit) const
{
    if (!tableExists(QStringLiteral("call_graph")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT cg.caller_func, COALESCE(f.path, '') "
        "FROM call_graph cg "
        "LEFT JOIN files f ON cg.caller_file_id = f.id "
        "WHERE cg.callee_func = ? "
        "ORDER BY cg.caller_func LIMIT ?"));
    q.bindValue(0, funcName);
    q.bindValue(1, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("caller")] = q.value(0).toString();
        item[QStringLiteral("file")]   = q.value(1).toString();
        arr.append(item);
    }
    return arr;
}

// ── hotspots ───────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::hotspots(int limit) const
{
    if (!tableExists(QStringLiteral("hotspot_scores")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT f.path, hs.hotspot_score, hs.coupling_in, hs.coupling_out, "
        "hs.has_tests, hs.crash_indicators, hs.risk_factors "
        "FROM hotspot_scores hs JOIN files f ON hs.file_id = f.id "
        "ORDER BY hs.hotspot_score DESC LIMIT ?"));
    q.bindValue(0, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("file")]             = q.value(0).toString();
        item[QStringLiteral("score")]            = q.value(1).toDouble();
        item[QStringLiteral("coupling_in")]      = q.value(2).toInt();
        item[QStringLiteral("coupling_out")]     = q.value(3).toInt();
        item[QStringLiteral("has_tests")]        = q.value(4).toBool();
        item[QStringLiteral("crash_indicators")] = q.value(5).toInt();
        item[QStringLiteral("risk_factors")]     = q.value(6).toString();
        arr.append(item);
    }
    return arr;
}

// ── deadCode ───────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::deadCode(int limit) const
{
    if (!tableExists(QStringLiteral("reachability")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT r.symbol, r.symbol_type, COALESCE(f.path, '') "
        "FROM reachability r "
        "LEFT JOIN files f ON r.file_id = f.id "
        "WHERE r.is_reachable = 0 "
        "ORDER BY f.path, r.symbol LIMIT ?"));
    q.bindValue(0, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("symbol")] = q.value(0).toString();
        item[QStringLiteral("type")]   = q.value(1).toString();
        item[QStringLiteral("file")]   = q.value(2).toString();
        arr.append(item);
    }
    return arr;
}

// ── typos ───────────────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::typos(int limit) const
{
    if (!tableExists(QStringLiteral("symbol_aliases")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT sa.symbol_a, sa.symbol_b, sa.edit_distance, sa.alias_type, "
        "COALESCE(fa.path, '') AS file_a, COALESCE(fb.path, '') AS file_b "
        "FROM symbol_aliases sa "
        "LEFT JOIN files fa ON sa.file_a = fa.id "
        "LEFT JOIN files fb ON sa.file_b = fb.id "
        "WHERE sa.alias_type = 'typo' "
        "ORDER BY sa.edit_distance, sa.symbol_a LIMIT ?"));
    q.bindValue(0, limit);
    q.exec();

    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("symbol_a")]      = q.value(0).toString();
        item[QStringLiteral("symbol_b")]      = q.value(1).toString();
        item[QStringLiteral("edit_distance")] = q.value(2).toInt();
        item[QStringLiteral("type")]          = q.value(3).toString();
        item[QStringLiteral("file_a")]        = q.value(4).toString();
        item[QStringLiteral("file_b")]        = q.value(5).toString();
        arr.append(item);
    }
    return arr;
}

// ── analysisScores ───────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::analysisScores(int limit) const
{
    if (!tableExists(QStringLiteral("analysis_scores")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT symbol_name, file_path, hotness_score, complexity_score, "
        "fanin_score, fanout_score, gpu_score, ct_risk_score, "
        "audit_gap_score, overall_priority, reasons "
        "FROM analysis_scores ORDER BY overall_priority DESC LIMIT ?"));
    q.bindValue(0, limit);
    q.exec();
    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("symbol")]       = q.value(0).toString();
        item[QStringLiteral("file")]         = q.value(1).toString();
        item[QStringLiteral("hotness")]      = q.value(2).toInt();
        item[QStringLiteral("complexity")]   = q.value(3).toInt();
        item[QStringLiteral("fanin")]        = q.value(4).toInt();
        item[QStringLiteral("fanout")]       = q.value(5).toInt();
        item[QStringLiteral("gpu")]          = q.value(6).toInt();
        item[QStringLiteral("ct_risk")]      = q.value(7).toInt();
        item[QStringLiteral("audit_gap")]    = q.value(8).toInt();
        item[QStringLiteral("priority")]     = q.value(9).toInt();
        item[QStringLiteral("reasons")]      = q.value(10).toString();
        arr.append(item);
    }
    return arr;
}

// ── symbolSlice ──────────────────────────────────────────────────────────────

QJsonObject CodeGraphQuery::symbolSlice(const QString &symbol) const
{
    if (!tableExists(QStringLiteral("symbol_slices")))
        return {};

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT ss.symbol, ss.signature, ss.critical_lines, "
        "ss.slice_token_estimate, ss.full_token_estimate, f.path "
        "FROM symbol_slices ss JOIN files f ON ss.file_id = f.id "
        "WHERE ss.symbol = ? OR ss.symbol LIKE ? ORDER BY length(ss.symbol) LIMIT 1"));
    q.bindValue(0, symbol);
    q.bindValue(1, QLatin1Char('%') + symbol);
    q.exec();
    if (!q.next()) return {};

    QJsonObject obj;
    obj[QStringLiteral("symbol")]         = q.value(0).toString();
    obj[QStringLiteral("signature")]      = q.value(1).toString();
    obj[QStringLiteral("critical_lines")] = q.value(2).toString();
    obj[QStringLiteral("slice_tokens")]   = q.value(3).toInt();
    obj[QStringLiteral("full_tokens")]    = q.value(4).toInt();
    obj[QStringLiteral("file")]           = q.value(5).toString();

    if (q.value(4).toInt() > 0) {
        const double savings =
            (1.0 - static_cast<double>(q.value(3).toInt()) /
                   q.value(4).toInt()) * 100.0;
        obj[QStringLiteral("token_savings_pct")] =
            static_cast<int>(savings);
    }
    return obj;
}

// ── callChain ────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::callChain(const QString &symbol, int depth) const
{
    Q_UNUSED(depth)
    if (!tableExists(QStringLiteral("call_graph")))
        return {};

    QJsonArray arr;

    // callers
    QSqlQuery qCallers(m_db);
    qCallers.prepare(QStringLiteral(
        "SELECT DISTINCT cg.caller_func, COALESCE(f.path,'') "
        "FROM call_graph cg LEFT JOIN files f ON cg.caller_file = f.id "
        "WHERE cg.callee_func = ? OR cg.callee_func LIKE ? "
        "ORDER BY cg.caller_func"));
    qCallers.bindValue(0, symbol);
    qCallers.bindValue(1, QLatin1Char('%') + QStringLiteral("::") + symbol);
    qCallers.exec();
    while (qCallers.next()) {
        QJsonObject item;
        item[QStringLiteral("direction")] = QStringLiteral("caller");
        item[QStringLiteral("func")]      = qCallers.value(0).toString();
        item[QStringLiteral("file")]      = qCallers.value(1).toString();
        arr.append(item);
    }

    // callees
    QSqlQuery qCallees(m_db);
    qCallees.prepare(QStringLiteral(
        "SELECT DISTINCT cg.callee_func, COALESCE(f.path,'') "
        "FROM call_graph cg LEFT JOIN files f ON cg.callee_file = f.id "
        "WHERE cg.caller_func = ? OR cg.caller_func LIKE ? "
        "ORDER BY cg.callee_func"));
    qCallees.bindValue(0, symbol);
    qCallees.bindValue(1, QLatin1Char('%') + QStringLiteral("::") + symbol);
    qCallees.exec();
    while (qCallees.next()) {
        QJsonObject item;
        item[QStringLiteral("direction")] = QStringLiteral("callee");
        item[QStringLiteral("func")]      = qCallees.value(0).toString();
        item[QStringLiteral("file")]      = qCallees.value(1).toString();
        arr.append(item);
    }
    return arr;
}

// ── aiTasks ──────────────────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::aiTasks(const QString &taskType) const
{
    if (!tableExists(QStringLiteral("ai_tasks")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    if (taskType.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT task_type, symbol_name, file_path, prompt, status, priority, created_at "
            "FROM ai_tasks ORDER BY priority DESC"));
    } else {
        q.prepare(QStringLiteral(
            "SELECT task_type, symbol_name, file_path, prompt, status, priority, created_at "
            "FROM ai_tasks WHERE task_type = ? ORDER BY priority DESC"));
        q.bindValue(0, taskType);
    }
    q.exec();
    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("type")]       = q.value(0).toString();
        item[QStringLiteral("symbol")]     = q.value(1).toString();
        item[QStringLiteral("file")]       = q.value(2).toString();
        item[QStringLiteral("prompt")]     = q.value(3).toString();
        item[QStringLiteral("status")]     = q.value(4).toString();
        item[QStringLiteral("priority")]   = q.value(5).toInt();
        item[QStringLiteral("created_at")] = q.value(6).toString();
        arr.append(item);
    }
    return arr;
}

// ── duplicateFunctions ───────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::duplicateFunctions() const
{
    if (!tableExists(QStringLiteral("function_index")))
        return {};

    QJsonArray groups;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT DISTINCT duplicate_group FROM function_index "
        "WHERE duplicate_group != '' ORDER BY duplicate_group"));
    while (q.next()) {
        const QString group = q.value(0).toString();
        QJsonArray members;
        QSqlQuery qm(m_db);
        qm.prepare(QStringLiteral(
            "SELECT fi.qualified_name, f.path, fi.line_count "
            "FROM function_index fi JOIN files f ON fi.file_id = f.id "
            "WHERE fi.duplicate_group = ? ORDER BY f.path"));
        qm.bindValue(0, group);
        qm.exec();
        while (qm.next()) {
            QJsonObject item;
            item[QStringLiteral("name")]       = qm.value(0).toString();
            item[QStringLiteral("file")]       = qm.value(1).toString();
            item[QStringLiteral("line_count")] = qm.value(2).toInt();
            members.append(item);
        }
        QJsonObject groupObj;
        groupObj[QStringLiteral("group")]   = group;
        groupObj[QStringLiteral("members")] = members;
        groups.append(groupObj);
    }
    return groups;
}

// ── optimizationPatterns ─────────────────────────────────────────────────────

QJsonArray CodeGraphQuery::optimizationPatterns() const
{
    if (!tableExists(QStringLiteral("optimization_patterns")))
        return {};

    QJsonArray arr;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT pattern_name, description, gain, risk, applicable_when, example_symbol "
        "FROM optimization_patterns ORDER BY gain DESC, pattern_name"));
    while (q.next()) {
        QJsonObject item;
        item[QStringLiteral("name")]            = q.value(0).toString();
        item[QStringLiteral("description")]     = q.value(1).toString();
        item[QStringLiteral("gain")]            = q.value(2).toString();
        item[QStringLiteral("risk")]            = q.value(3).toString();
        item[QStringLiteral("applicable_when")] = q.value(4).toString();
        item[QStringLiteral("example")]         = q.value(5).toString();
        arr.append(item);
    }
    return arr;
}
