#include "projectbraindb.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>
#include <QUuid>

// ── path helpers ──────────────────────────────────────────────────────────────

QString ProjectBrainDbTool::dbPath() const
{
    return m_workspaceRoot + QStringLiteral("/.exorcist/project.db");
}

bool ProjectBrainDbTool::ensureDir() const
{
    return QDir().mkpath(m_workspaceRoot + QStringLiteral("/.exorcist"));
}

// ── spec ──────────────────────────────────────────────────────────────────────

ToolSpec ProjectBrainDbTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("project_brain_db");
    s.description = QStringLiteral(
        "Workspace code index stored in .exorcist/project.db (SQLite).\n"
        "Indexes files, symbols, dependencies, modules, and project knowledge.\n\n"
        "Operations:\n"
        "  'init'    — create schema + full workspace scan (run once per project)\n"
        "  'scan'    — incremental scan: only re-index changed files (by hash)\n"
        "  'query'   — run SELECT queries against the index\n"
        "  'learn'   — store a knowledge fact (key/value/category)\n"
        "  'forget'  — delete a knowledge fact by key\n"
        "  'status'  — overview stats (file count, symbol count, staleness)\n"
        "  'execute' — run DDL/DML for schema extensions\n\n"
        "Tables: files, symbols, dependencies, modules, file_modules, knowledge.\n"
        "Useful queries:\n"
        "  SELECT name, kind, file_path FROM symbols WHERE name LIKE '%Foo%'\n"
        "  SELECT * FROM knowledge WHERE category='architecture'\n"
        "  SELECT * FROM files WHERE language='cpp' ORDER BY line_count DESC\n"
        "  SELECT source_file, target_file FROM dependencies WHERE kind='include'");
    s.permission   = AgentToolPermission::SafeMutate;
    s.timeoutMs    = 60000;
    s.parallelSafe = false;
    s.inputSchema  = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: init, scan, query, learn, forget, status, execute")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("init"), QStringLiteral("scan"),
                    QStringLiteral("query"), QStringLiteral("learn"),
                    QStringLiteral("forget"), QStringLiteral("status"),
                    QStringLiteral("execute")}}
            }},
            {QStringLiteral("sql"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("SQL for query/execute operations.")}
            }},
            {QStringLiteral("key"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Knowledge key (for learn/forget).")}
            }},
            {QStringLiteral("value"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Knowledge value (for learn).")}
            }},
            {QStringLiteral("category"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Knowledge category: architecture, pattern, "
                                "convention, gotcha, build, test")}
            }},
            {QStringLiteral("maxRows"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Max rows for query (default: 200, max: 2000).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

// ── invoke ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::invoke(const QJsonObject &args)
{
    if (m_workspaceRoot.isEmpty())
        return {false, {}, {},
            QStringLiteral("No workspace open.")};

    const QString op = args[QLatin1String("operation")].toString();

    if (op == QLatin1String("init"))
        return doInit();
    if (op == QLatin1String("scan"))
        return doScan();
    if (op == QLatin1String("status"))
        return doStatus();
    if (op == QLatin1String("query")) {
        const int maxRows = qBound(1, args[QLatin1String("maxRows")].toInt(200), 2000);
        return doQuery(args[QLatin1String("sql")].toString(), maxRows);
    }
    if (op == QLatin1String("execute"))
        return doExecute(args[QLatin1String("sql")].toString());
    if (op == QLatin1String("learn"))
        return doLearn(args[QLatin1String("key")].toString(),
                       args[QLatin1String("value")].toString(),
                       args[QLatin1String("category")].toString());
    if (op == QLatin1String("forget"))
        return doForget(args[QLatin1String("key")].toString());

    return {false, {}, {},
        QStringLiteral("Unknown operation: '%1'.").arg(op)};
}

// ── schema creation ───────────────────────────────────────────────────────────

bool ProjectBrainDbTool::createSchema(QSqlDatabase &db)
{
    const QStringList ddl = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS files ("
            "  path        TEXT PRIMARY KEY,"
            "  language    TEXT,"
            "  size_bytes  INTEGER,"
            "  line_count  INTEGER,"
            "  last_hash   TEXT,"
            "  last_scan   TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS symbols ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  file_path   TEXT NOT NULL,"
            "  name        TEXT NOT NULL,"
            "  kind        TEXT,"
            "  line        INTEGER,"
            "  parent      TEXT,"
            "  signature   TEXT,"
            "  FOREIGN KEY (file_path) REFERENCES files(path) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS dependencies ("
            "  source_file TEXT NOT NULL,"
            "  target_file TEXT NOT NULL,"
            "  kind        TEXT NOT NULL,"
            "  PRIMARY KEY (source_file, target_file, kind)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS modules ("
            "  name        TEXT PRIMARY KEY,"
            "  description TEXT,"
            "  root_path   TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS file_modules ("
            "  file_path   TEXT NOT NULL,"
            "  module_name TEXT NOT NULL,"
            "  PRIMARY KEY (file_path, module_name)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS knowledge ("
            "  key         TEXT PRIMARY KEY,"
            "  value       TEXT,"
            "  category    TEXT,"
            "  updated     TEXT"
            ")"),
        // Indexes
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbols_name ON symbols(name)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbols_kind ON symbols(kind)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_symbols_file ON symbols(file_path)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_deps_source ON dependencies(source_file)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_deps_target ON dependencies(target_file)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_knowledge_cat ON knowledge(category)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_file_modules_mod ON file_modules(module_name)"),
        // Enable foreign key enforcement
        QStringLiteral("PRAGMA foreign_keys = ON"),
    };

    for (const QString &sql : ddl) {
        QSqlQuery q(db);
        if (!q.exec(sql))
            return false;
    }
    return true;
}

// ── init ──────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doInit()
{
    if (!ensureDir())
        return {false, {}, {},
            QStringLiteral("Failed to create .exorcist/ directory.")};

    const QString path = dbPath();
    const bool alreadyExists = QFileInfo::exists(path);

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);

        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else if (!createSchema(db)) {
            result = {false, {}, {},
                QStringLiteral("Failed to create schema.")};
        } else {
            const int scanned = scanFiles(db, true);

            // Auto-detect modules from directory structure
            QSqlQuery q(db);
            q.exec(QStringLiteral(
                "INSERT OR IGNORE INTO modules (name, root_path) VALUES "
                "('editor', 'src/editor/'), ('agent', 'src/agent/'), "
                "('terminal', 'src/terminal/'), ('build', 'src/build/'), "
                "('debug', 'src/debug/'), ('git', 'src/git/'), "
                "('lsp', 'src/lsp/'), ('search', 'src/search/'), "
                "('project', 'src/project/'), ('mcp', 'src/mcp/'), "
                "('remote', 'src/remote/'), ('core', 'src/core/'), "
                "('ui', 'src/ui/'), ('plugins', 'plugins/'), "
                "('server', 'server/'), ('tests', 'tests/')"));

            // Auto-assign files to modules
            q.exec(QStringLiteral(
                "INSERT OR IGNORE INTO file_modules (file_path, module_name) "
                "SELECT f.path, m.name FROM files f, modules m "
                "WHERE f.path LIKE m.root_path || '%'"));

            const QString msg = alreadyExists
                ? QStringLiteral("Re-initialized project.db: %1 files scanned.")
                      .arg(scanned)
                : QStringLiteral("Created project.db: %1 files scanned.")
                      .arg(scanned);

            QJsonObject data;
            data[QLatin1String("filesScanned")] = scanned;
            data[QLatin1String("dbPath")]        = path;
            result = {true, data, msg, {}};
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── scan (incremental) ───────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doScan()
{
    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {false, {}, {},
            QStringLiteral("project.db not found. Run 'init' first.")};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);

        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else {
            const int updated = scanFiles(db, false);
            QJsonObject data;
            data[QLatin1String("filesUpdated")] = updated;
            result = {true, data,
                QStringLiteral("Incremental scan complete: %1 file(s) updated.")
                    .arg(updated), {}};
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── file scanning engine ─────────────────────────────────────────────────────

int ProjectBrainDbTool::scanFiles(QSqlDatabase &db, bool fullScan)
{
    // Collect existing hashes for incremental mode
    QHash<QString, QString> existingHashes;
    if (!fullScan) {
        QSqlQuery hashQ(db);
        hashQ.exec(QStringLiteral("SELECT path, last_hash FROM files"));
        while (hashQ.next())
            existingHashes[hashQ.value(0).toString()] = hashQ.value(1).toString();
    }

    // BFS workspace with depth limit
    struct Level { QString absPath; QString relBase; int depth; };
    QVector<Level> queue;
    queue.append({m_workspaceRoot, QString(), 0});

    constexpr int kMaxDepth = 8;
    constexpr int kMaxFiles = 20000;
    int scanned = 0;
    int totalFiles = 0;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    db.transaction();

    while (!queue.isEmpty() && totalFiles < kMaxFiles) {
        const auto cur = queue.takeFirst();
        QDir dir(cur.absPath);
        const auto entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QFileInfo &fi : entries) {
            if (totalFiles >= kMaxFiles)
                break;

            if (fi.isDir()) {
                if (shouldSkipDir(fi.fileName()))
                    continue;
                if (cur.depth < kMaxDepth) {
                    const QString rel = cur.relBase.isEmpty()
                        ? fi.fileName()
                        : cur.relBase + QLatin1Char('/') + fi.fileName();
                    queue.append({fi.absoluteFilePath(), rel, cur.depth + 1});
                }
                continue;
            }

            const QString lang = detectLanguage(fi.fileName());
            if (lang.isEmpty())
                continue; // skip non-code files

            ++totalFiles;
            const QString relPath = cur.relBase.isEmpty()
                ? fi.fileName()
                : cur.relBase + QLatin1Char('/') + fi.fileName();

            // Compute file hash
            QFile f(fi.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly))
                continue;
            const QByteArray content = f.readAll();
            f.close();

            const QString hash = QString::fromLatin1(
                QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());

            // Skip if unchanged
            if (!fullScan && existingHashes.value(relPath) == hash)
                continue;

            // Count lines
            int lineCount = 0;
            for (const char c : content) {
                if (c == '\n')
                    ++lineCount;
            }
            if (!content.isEmpty() && content.back() != '\n')
                ++lineCount;

            // Upsert file record
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO files (path, language, size_bytes, "
                "line_count, last_hash, last_scan) "
                "VALUES (?, ?, ?, ?, ?, ?)"));
            q.addBindValue(relPath);
            q.addBindValue(lang);
            q.addBindValue(fi.size());
            q.addBindValue(lineCount);
            q.addBindValue(hash);
            q.addBindValue(now);
            q.exec();

            // Re-extract symbols and includes
            scanSingleFile(db, relPath, fi.absoluteFilePath());
            ++scanned;
        }
    }

    // Remove deleted files
    if (fullScan) {
        // On full scan, remove files that no longer exist
        QSqlQuery delCheck(db);
        delCheck.exec(QStringLiteral("SELECT path FROM files"));
        QStringList toDelete;
        while (delCheck.next()) {
            const QString rel = delCheck.value(0).toString();
            if (!QFileInfo::exists(m_workspaceRoot + QLatin1Char('/') + rel))
                toDelete << rel;
        }
        for (const QString &rel : toDelete) {
            QSqlQuery d(db);
            d.prepare(QStringLiteral("DELETE FROM files WHERE path = ?"));
            d.addBindValue(rel);
            d.exec();
            d.prepare(QStringLiteral("DELETE FROM symbols WHERE file_path = ?"));
            d.addBindValue(rel);
            d.exec();
            d.prepare(QStringLiteral("DELETE FROM dependencies WHERE source_file = ?"));
            d.addBindValue(rel);
            d.exec();
        }
    }

    db.commit();
    return scanned;
}

void ProjectBrainDbTool::scanSingleFile(QSqlDatabase &db, const QString &relPath,
                                        const QString &absPath)
{
    // Clear old symbols/deps for this file
    QSqlQuery delSym(db);
    delSym.prepare(QStringLiteral("DELETE FROM symbols WHERE file_path = ?"));
    delSym.addBindValue(relPath);
    delSym.exec();

    QSqlQuery delDep(db);
    delDep.prepare(QStringLiteral("DELETE FROM dependencies WHERE source_file = ?"));
    delDep.addBindValue(relPath);
    delDep.exec();

    QFile f(absPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    const QString lang = detectLanguage(relPath);
    extractSymbols(db, relPath, content, lang);

    if (lang == QLatin1String("cpp") || lang == QLatin1String("c"))
        extractIncludes(db, relPath, content);
}

// ── symbol extraction (regex-based, fast) ────────────────────────────────────

void ProjectBrainDbTool::extractSymbols(QSqlDatabase &db, const QString &filePath,
                                        const QString &content, const QString &language)
{
    // C/C++ patterns
    static const QRegularExpression rxClass(
        QStringLiteral(R"((?:^|\n)\s*(?:class|struct)\s+(\w+)(?:\s*[:{]))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxEnum(
        QStringLiteral(R"((?:^|\n)\s*enum\s+(?:class\s+)?(\w+))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxFunction(
        QStringLiteral(R"((?:^|\n)(?:[\w:*&<>\s]+?)\s+(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?(?:=\s*0\s*)?[{;])"),
        QRegularExpression::MultilineOption);

    // Python patterns
    static const QRegularExpression rxPyClass(
        QStringLiteral(R"((?:^|\n)class\s+(\w+))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxPyDef(
        QStringLiteral(R"((?:^|\n)\s*def\s+(\w+)\s*\()"),
        QRegularExpression::MultilineOption);

    // JS/TS patterns
    static const QRegularExpression rxJsFunc(
        QStringLiteral(R"((?:^|\n)\s*(?:export\s+)?(?:async\s+)?function\s+(\w+))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxJsClass(
        QStringLiteral(R"((?:^|\n)\s*(?:export\s+)?class\s+(\w+))"),
        QRegularExpression::MultilineOption);

    // Rust patterns
    static const QRegularExpression rxRsFn(
        QStringLiteral(R"((?:^|\n)\s*(?:pub\s+)?fn\s+(\w+))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxRsStruct(
        QStringLiteral(R"((?:^|\n)\s*(?:pub\s+)?struct\s+(\w+))"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression rxRsEnum(
        QStringLiteral(R"((?:^|\n)\s*(?:pub\s+)?enum\s+(\w+))"),
        QRegularExpression::MultilineOption);

    struct Pattern {
        const QRegularExpression *rx;
        const char *kind;
    };

    QVector<Pattern> patterns;

    if (language == QLatin1String("cpp") || language == QLatin1String("c")) {
        patterns = {{&rxClass, "class"}, {&rxEnum, "enum"}, {&rxFunction, "function"}};
    } else if (language == QLatin1String("python")) {
        patterns = {{&rxPyClass, "class"}, {&rxPyDef, "function"}};
    } else if (language == QLatin1String("javascript") || language == QLatin1String("typescript")) {
        patterns = {{&rxJsClass, "class"}, {&rxJsFunc, "function"}};
    } else if (language == QLatin1String("rust")) {
        patterns = {{&rxRsStruct, "struct"}, {&rxRsEnum, "enum"}, {&rxRsFn, "function"}};
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO symbols (file_path, name, kind, line) VALUES (?, ?, ?, ?)"));

    for (const auto &pat : patterns) {
        auto it = pat.rx->globalMatch(content);
        while (it.hasNext()) {
            const auto match = it.next();
            const QString name = match.captured(1);

            // Skip common false positives
            if (name == QLatin1String("if") || name == QLatin1String("for")
                || name == QLatin1String("while") || name == QLatin1String("switch")
                || name == QLatin1String("return") || name == QLatin1String("else"))
                continue;

            // Compute line number
            const int pos = match.capturedStart(1);
            int line = 1;
            for (int i = 0; i < pos && i < content.size(); ++i) {
                if (content[i] == QLatin1Char('\n'))
                    ++line;
            }

            q.addBindValue(filePath);
            q.addBindValue(name);
            q.addBindValue(QString::fromLatin1(pat.kind));
            q.addBindValue(line);
            q.exec();
        }
    }
}

// ── include extraction (C/C++) ───────────────────────────────────────────────

void ProjectBrainDbTool::extractIncludes(QSqlDatabase &db, const QString &filePath,
                                         const QString &content)
{
    static const QRegularExpression rxInclude(
        QStringLiteral(R"(#\s*include\s*["<]([^">]+)[">])"),
        QRegularExpression::MultilineOption);

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO dependencies (source_file, target_file, kind) "
        "VALUES (?, ?, 'include')"));

    auto it = rxInclude.globalMatch(content);
    while (it.hasNext()) {
        const auto match = it.next();
        q.addBindValue(filePath);
        q.addBindValue(match.captured(1));
        q.exec();
    }
}

// ── language detection ───────────────────────────────────────────────────────

QString ProjectBrainDbTool::detectLanguage(const QString &path) const
{
    static const QHash<QString, QString> extMap = {
        {QStringLiteral("cpp"), QStringLiteral("cpp")},
        {QStringLiteral("cxx"), QStringLiteral("cpp")},
        {QStringLiteral("cc"), QStringLiteral("cpp")},
        {QStringLiteral("c"), QStringLiteral("c")},
        {QStringLiteral("h"), QStringLiteral("cpp")},
        {QStringLiteral("hpp"), QStringLiteral("cpp")},
        {QStringLiteral("hxx"), QStringLiteral("cpp")},
        {QStringLiteral("py"), QStringLiteral("python")},
        {QStringLiteral("js"), QStringLiteral("javascript")},
        {QStringLiteral("mjs"), QStringLiteral("javascript")},
        {QStringLiteral("ts"), QStringLiteral("typescript")},
        {QStringLiteral("tsx"), QStringLiteral("typescript")},
        {QStringLiteral("rs"), QStringLiteral("rust")},
        {QStringLiteral("go"), QStringLiteral("go")},
        {QStringLiteral("java"), QStringLiteral("java")},
        {QStringLiteral("kt"), QStringLiteral("kotlin")},
        {QStringLiteral("rb"), QStringLiteral("ruby")},
        {QStringLiteral("php"), QStringLiteral("php")},
        {QStringLiteral("lua"), QStringLiteral("lua")},
        {QStringLiteral("cs"), QStringLiteral("csharp")},
        {QStringLiteral("swift"), QStringLiteral("swift")},
        {QStringLiteral("dart"), QStringLiteral("dart")},
        {QStringLiteral("scala"), QStringLiteral("scala")},
        {QStringLiteral("cmake"), QStringLiteral("cmake")},
        {QStringLiteral("json"), QStringLiteral("json")},
        {QStringLiteral("yaml"), QStringLiteral("yaml")},
        {QStringLiteral("yml"), QStringLiteral("yaml")},
        {QStringLiteral("toml"), QStringLiteral("toml")},
        {QStringLiteral("md"), QStringLiteral("markdown")},
        {QStringLiteral("txt"), QStringLiteral("text")},
    };

    const QString ext = QFileInfo(path).suffix().toLower();
    auto it = extMap.constFind(ext);
    if (it != extMap.constEnd())
        return it.value();

    // Handle CMakeLists.txt (no extension)
    if (path.endsWith(QLatin1String("CMakeLists.txt")))
        return QStringLiteral("cmake");

    return {};
}

// ── skip directories ─────────────────────────────────────────────────────────

bool ProjectBrainDbTool::shouldSkipDir(const QString &name) const
{
    return name.startsWith(QLatin1Char('.'))
        || name == QLatin1String("node_modules")
        || name == QLatin1String("build")
        || name == QLatin1String("dist")
        || name == QLatin1String("__pycache__")
        || name == QLatin1String("target")
        || name == QLatin1String("third_party")
        || name == QLatin1String("ReserchRepos")
        || name.startsWith(QLatin1String("build-"));
}

// ── query ─────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doQuery(const QString &sql, int maxRows)
{
    if (sql.isEmpty())
        return {false, {}, {},
            QStringLiteral("'sql' is required for 'query'.")};

    // Block writes in query mode
    const QString upper = sql.trimmed().toUpper();
    if (upper.startsWith(QLatin1String("INSERT"))
        || upper.startsWith(QLatin1String("UPDATE"))
        || upper.startsWith(QLatin1String("DELETE"))
        || upper.startsWith(QLatin1String("DROP"))
        || upper.startsWith(QLatin1String("ALTER"))
        || upper.startsWith(QLatin1String("CREATE")))
        return {false, {}, {},
            QStringLiteral("Write operations not allowed in 'query'. "
                           "Use 'execute' instead.")};

    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {false, {}, {},
            QStringLiteral("project.db not found. Run 'init' first.")};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else {
            QSqlQuery q(db);
            if (!q.exec(sql)) {
                result = {false, {}, {},
                    QStringLiteral("SQL error: %1").arg(q.lastError().text())};
            } else {
                result = {true, {}, formatResults(q, maxRows), {}};
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── learn ─────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doLearn(const QString &key, const QString &value,
                                           const QString &category)
{
    if (key.isEmpty() || value.isEmpty())
        return {false, {}, {},
            QStringLiteral("'key' and 'value' are required for 'learn'.")};

    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {false, {}, {},
            QStringLiteral("project.db not found. Run 'init' first.")};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO knowledge (key, value, category, updated) "
                "VALUES (?, ?, ?, ?)"));
            q.addBindValue(key);
            q.addBindValue(value);
            q.addBindValue(category.isEmpty() ? QStringLiteral("general") : category);
            q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            if (!q.exec()) {
                result = {false, {}, {},
                    QStringLiteral("Failed to store knowledge: %1")
                        .arg(q.lastError().text())};
            } else {
                result = {true, {},
                    QStringLiteral("Learned: [%1] %2 = %3").arg(
                        category.isEmpty() ? QStringLiteral("general") : category,
                        key, value), {}};
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── forget ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doForget(const QString &key)
{
    if (key.isEmpty())
        return {false, {}, {},
            QStringLiteral("'key' is required for 'forget'.")};

    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {false, {}, {},
            QStringLiteral("project.db not found. Run 'init' first.")};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("DELETE FROM knowledge WHERE key = ?"));
            q.addBindValue(key);
            if (!q.exec()) {
                result = {false, {}, {},
                    QStringLiteral("Failed: %1").arg(q.lastError().text())};
            } else {
                const int n = q.numRowsAffected();
                result = {true, {},
                    n > 0
                        ? QStringLiteral("Forgot: '%1'").arg(key)
                        : QStringLiteral("Key '%1' not found.").arg(key), {}};
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── status ────────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doStatus()
{
    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {true, {},
            QStringLiteral("No project.db found. Run 'init' to create one."), {}};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database.")};
        } else {
            auto countOf = [&](const QString &table) -> int {
                QSqlQuery q(db);
                q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table));
                return q.next() ? q.value(0).toInt() : 0;
            };

            const int files    = countOf(QStringLiteral("files"));
            const int symbols  = countOf(QStringLiteral("symbols"));
            const int deps     = countOf(QStringLiteral("dependencies"));
            const int modules  = countOf(QStringLiteral("modules"));
            const int facts    = countOf(QStringLiteral("knowledge"));

            // Languages breakdown
            QSqlQuery langQ(db);
            langQ.exec(QStringLiteral(
                "SELECT language, COUNT(*) as cnt FROM files "
                "GROUP BY language ORDER BY cnt DESC LIMIT 10"));
            QStringList langLines;
            while (langQ.next())
                langLines << QStringLiteral("  %1: %2")
                    .arg(langQ.value(0).toString())
                    .arg(langQ.value(1).toInt());

            // Last scan time
            QSqlQuery scanQ(db);
            scanQ.exec(QStringLiteral(
                "SELECT MAX(last_scan) FROM files"));
            const QString lastScan = scanQ.next()
                ? scanQ.value(0).toString() : QStringLiteral("never");

            // DB file size
            const qint64 dbSize = QFileInfo(path).size();

            QJsonObject data;
            data[QLatin1String("files")]    = files;
            data[QLatin1String("symbols")]  = symbols;
            data[QLatin1String("deps")]     = deps;
            data[QLatin1String("modules")]  = modules;
            data[QLatin1String("facts")]    = facts;
            data[QLatin1String("lastScan")] = lastScan;
            data[QLatin1String("dbSizeKB")] = static_cast<int>(dbSize / 1024);

            const QString text = QStringLiteral(
                "Project Brain: .exorcist/project.db\n"
                "─────────────────────────────────\n"
                "Files:        %1\n"
                "Symbols:      %2\n"
                "Dependencies: %3\n"
                "Modules:      %4\n"
                "Knowledge:    %5 facts\n"
                "Last scan:    %6\n"
                "DB size:      %7 KB\n\n"
                "Languages:\n%8")
                .arg(files).arg(symbols).arg(deps).arg(modules).arg(facts)
                .arg(lastScan)
                .arg(dbSize / 1024)
                .arg(langLines.join(QLatin1Char('\n')));

            result = {true, data, text, {}};
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── execute ───────────────────────────────────────────────────────────────────

ToolExecResult ProjectBrainDbTool::doExecute(const QString &sql)
{
    if (sql.isEmpty())
        return {false, {}, {},
            QStringLiteral("'sql' is required for 'execute'.")};

    const QString path = dbPath();
    if (!QFileInfo::exists(path))
        return {false, {}, {},
            QStringLiteral("project.db not found. Run 'init' first.")};

    const QString connName = QStringLiteral("brain_%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(8));

    ToolExecResult result;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            result = {false, {}, {},
                QStringLiteral("Failed to open database: %1")
                    .arg(db.lastError().text())};
        } else {
            QSqlQuery q(db);
            if (!q.exec(sql)) {
                result = {false, {}, {},
                    QStringLiteral("SQL error: %1").arg(q.lastError().text())};
            } else {
                const int affected = q.numRowsAffected();
                result = {true, {},
                    QStringLiteral("Executed. %1 row(s) affected.").arg(affected), {}};
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    return result;
}

// ── format results ───────────────────────────────────────────────────────────

QString ProjectBrainDbTool::formatResults(QSqlQuery &q, int maxRows)
{
    const QSqlRecord rec = q.record();
    const int cols = rec.count();
    if (cols == 0)
        return QStringLiteral("(no results)");

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
        result += QStringLiteral("\n... (showing first %1)").arg(maxRows);

    result += QStringLiteral("\n\n%1 row(s) returned.").arg(rowCount);
    return result;
}
