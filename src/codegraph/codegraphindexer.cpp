#include "codegraphindexer.h"
#include "ilanguageindexer.h"
#include "cpplanguageindexer.h"
#include "tslanguageindexer.h"
#include "pylanguageindexer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QVariant>

// ══════════════════════════════════════════════════════════════════════════════
// CMake regex patterns (build-system specific, not language-specific)
// ══════════════════════════════════════════════════════════════════════════════

static const QRegularExpression RE_CMAKE_EXEC(
    QStringLiteral(R"(add_executable\s*\(\s*(\S+))"),
    QRegularExpression::MultilineOption);
static const QRegularExpression RE_CMAKE_LIB(
    QStringLiteral(R"(add_library\s*\(\s*(\S+))"),
    QRegularExpression::MultilineOption);
static const QRegularExpression RE_CMAKE_TEST(
    QStringLiteral(R"(add_test\s*\(\s*(?:NAME\s+)?(\S+))"),
    QRegularExpression::MultilineOption);

// Non-code extensions that are still indexed as files
static const QSet<QString> DATA_EXTS = {
    QStringLiteral(".json"), QStringLiteral(".md"),
};

// ── Helpers ──────────────────────────────────────────────────────────────────

static QString readFileContent(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    if (f.size() > 2'000'000)
        return {};
    return QString::fromUtf8(f.readAll());
}

// ══════════════════════════════════════════════════════════════════════════════
// Constructor — registers built-in language indexers
// ══════════════════════════════════════════════════════════════════════════════

CodeGraphIndexer::CodeGraphIndexer()
{
    registerLanguageIndexer(std::make_shared<CppLanguageIndexer>());
    registerLanguageIndexer(std::make_shared<TsLanguageIndexer>());
    registerLanguageIndexer(std::make_shared<PythonLanguageIndexer>());
}

void CodeGraphIndexer::setWorkspaceRoot(const QString &root)
{
    m_workspaceRoot = root;
}

void CodeGraphIndexer::registerLanguageIndexer(std::shared_ptr<ILanguageIndexer> indexer)
{
    const QString id = indexer->languageId();
    for (const auto &ext : indexer->fileExtensions())
        m_codeExtensions.insert(ext);
    m_indexers[id] = std::move(indexer);
}

ILanguageIndexer *CodeGraphIndexer::indexerForLang(const QString &lang) const
{
    auto it = m_indexers.find(lang);
    if (it != m_indexers.end())
        return it->get();
    // TypeScript uses "typescript" as languageId but detectLanguage may return
    // "javascript" — map both to the TS indexer.
    if (lang == QLatin1String("javascript")) {
        auto tsIt = m_indexers.find(QStringLiteral("typescript"));
        if (tsIt != m_indexers.end())
            return tsIt->get();
    }
    return nullptr;
}

// ── Static helpers ───────────────────────────────────────────────────────────

bool CodeGraphIndexer::shouldSkipDir(const QString &name)
{
    static const QSet<QString> skipSet = {
        QStringLiteral("node_modules"), QStringLiteral(".git"),
        QStringLiteral("build"),        QStringLiteral("build-llvm"),
        QStringLiteral("build-ci"),     QStringLiteral("build-release"),
        QStringLiteral("third_party"),  QStringLiteral("_deps"),
        QStringLiteral("__pycache__"),  QStringLiteral("CMakeFiles"),
        QStringLiteral(".vs"),          QStringLiteral(".vscode"),
    };
    if (skipSet.contains(name))
        return true;
    if (name.size() > 1 && name.startsWith(QLatin1Char('.')))
        return true;
    if (name.startsWith(QLatin1String("build-")))
        return true;
    return false;
}

QString CodeGraphIndexer::detectLanguage(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("h") || ext == QLatin1String("hpp") ||
        ext == QLatin1String("cpp") || ext == QLatin1String("cc") ||
        ext == QLatin1String("cxx"))
        return QStringLiteral("cpp");
    if (ext == QLatin1String("ts") || ext == QLatin1String("tsx"))
        return QStringLiteral("typescript");
    if (ext == QLatin1String("js") || ext == QLatin1String("mjs"))
        return QStringLiteral("javascript");
    if (ext == QLatin1String("py"))
        return QStringLiteral("python");
    if (ext == QLatin1String("json"))
        return QStringLiteral("json");
    if (ext == QLatin1String("md"))
        return QStringLiteral("markdown");
    return QStringLiteral("other");
}

QString CodeGraphIndexer::detectSubsystem(const QString &relPath)
{
    const auto parts = relPath.split(QLatin1Char('/'));
    if (parts.isEmpty()) return QStringLiteral("other");

    if (parts[0] == QLatin1String("src")) {
        if (parts.size() >= 3)
            return parts[1];
        return QStringLiteral("core");
    }
    if (parts[0] == QLatin1String("plugins")) {
        if (parts.size() >= 3)
            return QStringLiteral("plugin-") + parts[1];
        return QStringLiteral("plugins");
    }
    if (parts[0] == QLatin1String("server"))
        return QStringLiteral("server");
    if (parts[0] == QLatin1String("tests"))
        return QStringLiteral("tests");
    return QStringLiteral("other");
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 1: Scan files
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanFiles(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO files (path, dir, name, ext, lang, lines, size_bytes, "
        "has_qobject, has_signals, has_slots, subsystem) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)"));

    const QDir root(m_workspaceRoot);
    QDirIterator it(m_workspaceRoot, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        {
            const QString relDir = root.relativeFilePath(fi.absolutePath())
                                       .replace(QLatin1Char('\\'), QLatin1Char('/'));
            bool skip = false;
            for (const auto &part : relDir.split(QLatin1Char('/'))) {
                if (shouldSkipDir(part)) { skip = true; break; }
            }
            if (skip) continue;
        }

        const QString ext = QLatin1Char('.') + fi.suffix().toLower();
        if (!m_codeExtensions.contains(ext) && !DATA_EXTS.contains(ext))
            continue;

        const QString relPath = root.relativeFilePath(fi.absoluteFilePath())
                                    .replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString dirPart = root.relativeFilePath(fi.absolutePath())
                                    .replace(QLatin1Char('\\'), QLatin1Char('/'));

        const qint64 size = fi.size();
        if (size > 2'000'000)
            continue;

        const QString content = readFileContent(fi.absoluteFilePath());
        if (content.isNull())
            continue;

        const int lineCount = content.count(QLatin1Char('\n')) + 1;
        const QString lang = detectLanguage(fi.fileName());

        // Delegate metadata extraction to the language indexer
        int hasQObj = 0, hasSig = 0, hasSlt = 0;
        if (auto *idx = indexerForLang(lang)) {
            auto meta = idx->extractFileMetadata(content);
            hasQObj = meta.hasQObject ? 1 : 0;
            hasSig  = meta.hasSignals ? 1 : 0;
            hasSlt  = meta.hasSlots   ? 1 : 0;
        }

        q.bindValue(0, relPath);
        q.bindValue(1, dirPart);
        q.bindValue(2, fi.fileName());
        q.bindValue(3, ext);
        q.bindValue(4, lang);
        q.bindValue(5, lineCount);
        q.bindValue(6, size);
        q.bindValue(7, hasQObj);
        q.bindValue(8, hasSig);
        q.bindValue(9, hasSlt);
        q.bindValue(10, detectSubsystem(relPath));
        if (q.exec())
            ++count;
    }

    m_stats.filesIndexed = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 2: Parse all files (classes, methods, includes)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::parseAll(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral("SELECT id, path, lang FROM files"));

    QSqlQuery incIns(db);
    incIns.prepare(QStringLiteral(
        "INSERT INTO includes (file_id, included, is_system) VALUES (?,?,?)"));

    QSqlQuery nsIns(db);
    nsIns.prepare(QStringLiteral(
        "INSERT INTO namespaces (file_id, name) VALUES (?,?)"));

    QSqlQuery clsIns(db);
    clsIns.prepare(QStringLiteral(
        "INSERT INTO classes (file_id, name, bases, line_num, "
        "is_interface, is_struct, lang) VALUES (?,?,?,?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        auto *idx = indexerForLang(lang);
        if (!idx) continue;

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        auto result = idx->parseFile(content);

        for (const auto &imp : result.imports) {
            incIns.bindValue(0, fileId);
            incIns.bindValue(1, imp.path);
            incIns.bindValue(2, imp.isSystem ? 1 : 0);
            incIns.exec();
        }

        for (const auto &ns : result.namespaces) {
            nsIns.bindValue(0, fileId);
            nsIns.bindValue(1, ns.name);
            nsIns.exec();
        }

        for (const auto &cls : result.classes) {
            clsIns.bindValue(0, fileId);
            clsIns.bindValue(1, cls.name);
            clsIns.bindValue(2, cls.bases);
            clsIns.bindValue(3, cls.lineNum);
            clsIns.bindValue(4, cls.isInterface ? 1 : 0);
            clsIns.bindValue(5, cls.isStruct ? 1 : 0);
            clsIns.bindValue(6, idx->languageId());
            clsIns.exec();
        }

        ++count;
    }

    m_stats.classesParsed = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 3: Build header → source implementation map
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildImplMap(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery headers(db);
    headers.exec(QStringLiteral(
        "SELECT id, path, name FROM files WHERE ext = '.h'"));

    while (headers.next()) {
        const qint64 hdrId    = headers.value(0).toLongLong();
        const QString hdrName = headers.value(2).toString();
        const QString base    = hdrName.left(hdrName.lastIndexOf(QLatin1Char('.')));

        QSqlQuery sources(db);
        sources.prepare(QStringLiteral(
            "SELECT id, path FROM files WHERE name LIKE ? AND ext = '.cpp'"));
        sources.bindValue(0, base + QStringLiteral(".cpp"));
        sources.exec();

        while (sources.next()) {
            const qint64 srcId    = sources.value(0).toLongLong();
            const QString srcPath = sources.value(1).toString();

            QSqlQuery classes(db);
            classes.prepare(QStringLiteral(
                "SELECT name FROM classes WHERE file_id = ?"));
            classes.bindValue(0, hdrId);
            classes.exec();

            while (classes.next()) {
                const QString clsName = classes.value(0).toString();
                const QString absPath = m_workspaceRoot + QLatin1Char('/') + srcPath;
                const QString srcContent = readFileContent(absPath);
                if (srcContent.isNull()) continue;

                const QString pattern = clsName + QStringLiteral("::");
                const int methodCount = srcContent.count(pattern);
                if (methodCount > 0) {
                    QSqlQuery ins(db);
                    ins.prepare(QStringLiteral(
                        "INSERT INTO implementations "
                        "(header_id, source_id, class_name, method_count) "
                        "VALUES (?,?,?,?)"));
                    ins.bindValue(0, hdrId);
                    ins.bindValue(1, srcId);
                    ins.bindValue(2, clsName);
                    ins.bindValue(3, methodCount);
                    ins.exec();

                    QSqlQuery upd(db);
                    upd.prepare(QStringLiteral(
                        "UPDATE classes SET has_impl = 1 "
                        "WHERE file_id = ? AND name = ?"));
                    upd.bindValue(0, hdrId);
                    upd.bindValue(1, clsName);
                    upd.exec();

                    ++count;
                }
            }
        }
    }

    m_stats.implsMapped = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 4: Build subsystem graph
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildSubsystemGraph(QSqlDatabase &db)
{
    QSqlQuery q(db);

    q.exec(QStringLiteral(
        "SELECT subsystem, COUNT(*), SUM(lines) "
        "FROM files WHERE lang = 'cpp' AND path LIKE 'src/%' "
        "GROUP BY subsystem ORDER BY SUM(lines) DESC"));

    int subCount = 0;
    while (q.next()) {
        const QString name  = q.value(0).toString();
        const int fileCount = q.value(1).toInt();
        const int lineCount = q.value(2).toInt();

        QSqlQuery cq(db);
        cq.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.lang = 'cpp'"));
        cq.bindValue(0, name);
        cq.exec(); cq.next();
        const int clsCount = cq.value(0).toInt();

        cq.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.has_impl = 0 AND c.is_interface = 0 "
            "AND c.lang = 'cpp' AND f.ext = '.h'"));
        cq.bindValue(0, name);
        cq.exec(); cq.next();
        const int hoCount = cq.value(0).toInt();

        cq.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM classes c JOIN files f ON c.file_id = f.id "
            "WHERE f.subsystem = ? AND c.is_interface = 1"));
        cq.bindValue(0, name);
        cq.exec(); cq.next();
        const int ifaceCount = cq.value(0).toInt();

        const QString dirPath = (name == QLatin1String("core"))
            ? QStringLiteral("src")
            : QStringLiteral("src/") + name;

        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO subsystems "
            "(name, dir_path, file_count, line_count, class_count, "
            "h_only_count, interface_count) VALUES (?,?,?,?,?,?,?)"));
        ins.bindValue(0, name);
        ins.bindValue(1, dirPath);
        ins.bindValue(2, fileCount);
        ins.bindValue(3, lineCount);
        ins.bindValue(4, clsCount);
        ins.bindValue(5, hoCount);
        ins.bindValue(6, ifaceCount);
        ins.exec();
        ++subCount;
    }

    // Subsystem dependency edges
    q.exec(QStringLiteral(
        "SELECT f1.subsystem, f2.subsystem, COUNT(*) "
        "FROM includes i "
        "JOIN files f1 ON i.file_id = f1.id "
        "JOIN files f2 ON f2.path LIKE '%/' || i.included "
        "              OR f2.path = i.included "
        "              OR f2.name = i.included "
        "WHERE f1.subsystem != '' AND f2.subsystem != '' "
        "  AND f1.subsystem != f2.subsystem "
        "  AND f1.path LIKE 'src/%' AND f2.path LIKE 'src/%' "
        "GROUP BY f1.subsystem, f2.subsystem"));

    while (q.next()) {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO subsystem_deps "
            "(from_subsystem, to_subsystem, edge_count) VALUES (?,?,?)"));
        ins.bindValue(0, q.value(0).toString());
        ins.bindValue(1, q.value(1).toString());
        ins.bindValue(2, q.value(2).toInt());
        ins.exec();
    }

    // Class cross-references via inheritance
    q.exec(QStringLiteral(
        "SELECT name, bases FROM classes "
        "WHERE bases IS NOT NULL AND bases != ''"));

    while (q.next()) {
        const QString clsName = q.value(0).toString();
        const auto baseList = q.value(1).toString().split(QLatin1Char(','));
        for (const QString &rawBase : baseList) {
            QString base = rawBase.trimmed();
            const int colonIdx = base.lastIndexOf(QLatin1String("::"));
            if (colonIdx >= 0)
                base = base.mid(colonIdx + 2);
            if (base.isEmpty() || base == clsName) continue;

            QSqlQuery ins(db);
            ins.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO class_refs "
                "(from_class, to_class, ref_type) VALUES (?,?,?)"));
            ins.bindValue(0, clsName);
            ins.bindValue(1, base);
            ins.bindValue(2, QStringLiteral("inherits"));
            ins.exec();
        }
    }

    return subCount;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 5: Build function index
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildFunctionIndex(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral(
        "SELECT id, path, lang FROM files WHERE ext != '.h'"));

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO function_index "
        "(file_id, name, qualified_name, params, return_type, "
        "start_line, end_line, line_count, is_method, class_name) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        auto *idx = indexerForLang(lang);
        if (!idx) continue;

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        auto funcs = idx->extractFunctions(content);

        for (const auto &f : funcs) {
            ins.bindValue(0, fileId);
            ins.bindValue(1, f.name);
            ins.bindValue(2, f.qualifiedName);
            ins.bindValue(3, f.params);
            ins.bindValue(4, f.returnType);
            ins.bindValue(5, f.startLine);
            ins.bindValue(6, f.endLine);
            ins.bindValue(7, f.lineCount);
            ins.bindValue(8, f.isMethod ? 1 : 0);
            ins.bindValue(9, f.className);
            ins.exec();
            ++count;
        }
    }

    m_stats.functionsExtracted = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 6: Build file summaries
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildFileSummaries(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral("SELECT id, path, lang FROM files"));

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO file_summaries (file_id, summary, category, key_classes) "
        "VALUES (?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        const QStringList lines = content.split(QLatin1Char('\n'));
        const QFileInfo fi(path);
        const QString ext = QLatin1Char('.') + fi.suffix().toLower();
        const QString baseName = fi.baseName().toLower();

        // ── Extract first comment via language indexer ──
        QString firstComment;
        if (auto *idx = indexerForLang(lang))
            firstComment = idx->extractFirstComment(lines);

        // ── Get class list for this file ──
        QSqlQuery clsQ(db);
        clsQ.prepare(QStringLiteral(
            "SELECT name, is_interface FROM classes WHERE file_id = ?"));
        clsQ.bindValue(0, fileId);
        clsQ.exec();

        QStringList classNames;
        bool hasInterface = false;
        bool hasWidget = false, hasService = false;
        while (clsQ.next()) {
            const QString cn = clsQ.value(0).toString();
            classNames << cn;
            if (clsQ.value(1).toInt() == 1) hasInterface = true;
            if (cn.contains(QLatin1String("Widget")) ||
                cn.contains(QLatin1String("Panel")) ||
                cn.contains(QLatin1String("View")) ||
                cn.contains(QLatin1String("Dialog")))
                hasWidget = true;
            if (cn.contains(QLatin1String("Manager")) ||
                cn.contains(QLatin1String("Service")) ||
                cn.contains(QLatin1String("Registry")) ||
                cn.contains(QLatin1String("Engine")))
                hasService = true;
        }

        // ── Categorize ──
        QString category = QStringLiteral("source");
        if (path.toLower().contains(QLatin1String("test")) ||
            baseName.startsWith(QLatin1String("test_")))
            category = QStringLiteral("test");
        else if (ext == QLatin1String(".h") || ext == QLatin1String(".hpp")) {
            if (hasInterface)       category = QStringLiteral("interface");
            else if (hasWidget)     category = QStringLiteral("widget");
            else if (hasService)    category = QStringLiteral("service");
            else                    category = QStringLiteral("header");
        } else if (ext == QLatin1String(".cpp") || ext == QLatin1String(".cc")) {
            if (hasWidget)      category = QStringLiteral("widget");
            else if (hasService) category = QStringLiteral("service");
            else                 category = QStringLiteral("implementation");
        } else if (ext == QLatin1String(".py"))
            category = QStringLiteral("tool");
        else if (ext == QLatin1String(".json"))
            category = QStringLiteral("config");
        else if (ext == QLatin1String(".md"))
            category = QStringLiteral("doc");

        // ── Summary ──
        QString summary;
        if (!firstComment.isEmpty() && firstComment.size() > 5) {
            summary = firstComment.left(200);
        } else if (!classNames.isEmpty()) {
            static const QHash<QString, QString> catLabels = {
                {QStringLiteral("interface"),      QStringLiteral("Interface")},
                {QStringLiteral("widget"),         QStringLiteral("UI widget")},
                {QStringLiteral("service"),        QStringLiteral("Service")},
                {QStringLiteral("test"),           QStringLiteral("Tests for")},
                {QStringLiteral("header"),         QStringLiteral("Header")},
                {QStringLiteral("implementation"), QStringLiteral("Implementation of")},
            };
            const QString label = catLabels.value(category, QStringLiteral("Defines"));
            summary = label + QStringLiteral(": ") +
                      classNames.mid(0, 5).join(QStringLiteral(", "));
        } else if (ext == QLatin1String(".md")) {
            for (const auto &ln : lines) {
                QString s = ln.trimmed();
                while (s.startsWith(QLatin1Char('#')))
                    s = s.mid(1).trimmed();
                if (!s.isEmpty()) {
                    summary = s.left(200);
                    break;
                }
            }
            if (summary.isEmpty())
                summary = fi.fileName();
        } else {
            summary = fi.fileName();
        }

        ins.bindValue(0, fileId);
        ins.bindValue(1, summary);
        ins.bindValue(2, category);
        ins.bindValue(3, classNames.mid(0, 10).join(QStringLiteral(", ")));
        ins.exec();
        ++count;
    }

    m_stats.summariesBuilt = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 7: Build unified edge graph
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildEdges(QSqlDatabase &db)
{
    int count = 0;

    QHash<QString, QVector<qint64>> nameToIds;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT id, name FROM files"));
        while (q.next())
            nameToIds[q.value(1).toString()].append(q.value(0).toLongLong());
    }

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO edges (source_file, target_file, source_name, "
        "target_name, edge_type, metadata) VALUES (?,?,?,?,?,?)"));

    // 1. Include edges
    QSet<QPair<qint64, qint64>> seen;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT file_id, included FROM includes WHERE is_system = 0"));
        while (q.next()) {
            const qint64 srcFid = q.value(0).toLongLong();
            const QString included = q.value(1).toString();
            const QString incName = included.mid(included.lastIndexOf(QLatin1Char('/')) + 1);
            const auto &targets = nameToIds.value(incName);
            for (qint64 tgtFid : targets) {
                if (tgtFid != srcFid && !seen.contains({srcFid, tgtFid})) {
                    seen.insert({srcFid, tgtFid});
                    ins.bindValue(0, srcFid);
                    ins.bindValue(1, tgtFid);
                    ins.bindValue(2, QString());
                    ins.bindValue(3, QString());
                    ins.bindValue(4, QStringLiteral("includes"));
                    ins.bindValue(5, QString());
                    ins.exec();
                    ++count;
                }
            }
        }
    }

    // 2. Implementation edges
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT header_id, source_id, class_name FROM implementations"));
        while (q.next()) {
            ins.bindValue(0, q.value(1));
            ins.bindValue(1, q.value(0));
            ins.bindValue(2, q.value(2));
            ins.bindValue(3, q.value(2));
            ins.bindValue(4, QStringLiteral("implements"));
            ins.bindValue(5, q.value(2));
            ins.exec();
            ++count;
        }
    }

    // 3. Inheritance edges
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT cr.from_class, cr.to_class, f1.id, f2.id "
            "FROM class_refs cr "
            "JOIN classes c1 ON c1.name = cr.from_class "
            "JOIN files f1 ON c1.file_id = f1.id "
            "JOIN classes c2 ON c2.name = cr.to_class "
            "JOIN files f2 ON c2.file_id = f2.id "
            "WHERE cr.ref_type = 'inherits'"));
        while (q.next()) {
            ins.bindValue(0, q.value(2));
            ins.bindValue(1, q.value(3));
            ins.bindValue(2, q.value(0));
            ins.bindValue(3, q.value(1));
            ins.bindValue(4, QStringLiteral("inherits"));
            ins.bindValue(5, QString());
            ins.exec();
            ++count;
        }
    }

    // 4. Test edges
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT id, name FROM files WHERE path LIKE 'tests/%' AND ext = '.cpp'"));
        while (q.next()) {
            const qint64 testFid = q.value(0).toLongLong();
            const QString testName = q.value(1).toString();
            QString base = testName;
            base.remove(QStringLiteral("test_"));
            base.remove(QStringLiteral(".cpp"));

            QSqlQuery tgt(db);
            tgt.prepare(QStringLiteral(
                "SELECT id FROM files WHERE path LIKE 'src/%' "
                "AND (name = ? OR name = ?)"));
            tgt.bindValue(0, base + QStringLiteral(".cpp"));
            tgt.bindValue(1, base + QStringLiteral(".h"));
            tgt.exec();
            while (tgt.next()) {
                ins.bindValue(0, testFid);
                ins.bindValue(1, tgt.value(0));
                ins.bindValue(2, testName);
                ins.bindValue(3, base);
                ins.bindValue(4, QStringLiteral("tests"));
                ins.bindValue(5, QString());
                ins.exec();
                ++count;
            }
        }
    }

    m_stats.edgesBuilt = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 8: Scan connections (via language indexers)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanQtConnections(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral("SELECT id, path, lang FROM files WHERE ext = '.cpp'"));

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO qt_connections "
        "(file_id, sender, signal_name, receiver, slot_name, line_num) "
        "VALUES (?,?,?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        auto *idx = indexerForLang(lang);
        if (!idx) continue;

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        auto conns = idx->scanConnections(content);
        for (const auto &c : conns) {
            ins.bindValue(0, fileId);
            ins.bindValue(1, c.sender);
            ins.bindValue(2, c.signalName);
            ins.bindValue(3, c.receiver);
            ins.bindValue(4, c.slotName);
            ins.bindValue(5, c.lineNum);
            ins.exec();
            ++count;
        }
    }

    m_stats.qtConnections = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 9: Scan service refs (via language indexers)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanServices(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral("SELECT id, path, lang FROM files"));

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO services (file_id, service_key, line_num, reg_type) "
        "VALUES (?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        auto *idx = indexerForLang(lang);
        if (!idx) continue;

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        auto refs = idx->scanServiceRefs(content);
        for (const auto &r : refs) {
            ins.bindValue(0, fileId);
            ins.bindValue(1, r.key);
            ins.bindValue(2, r.lineNum);
            ins.bindValue(3, r.regType);
            ins.exec();
            ++count;
        }
    }

    m_stats.servicesFound = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 10: Scan test cases (via language indexers)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanTestCases(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery sel(db);
    sel.exec(QStringLiteral(
        "SELECT id, path, lang FROM files WHERE path LIKE 'tests/%'"));

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO test_cases "
        "(file_id, test_class, test_method, line_num) VALUES (?,?,?,?)"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();
        const QString lang  = sel.value(2).toString();

        auto *idx = indexerForLang(lang);
        if (!idx) continue;

        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;
        const QString content = readFileContent(absPath);
        if (content.isNull()) continue;

        auto cases = idx->scanTestCases(content);

        // Fallback: if no class name from the language indexer, query DB
        QString fallbackClass;
        if (!cases.isEmpty() && cases.first().className.isEmpty()) {
            QSqlQuery cls(db);
            cls.prepare(QStringLiteral(
                "SELECT name FROM classes WHERE file_id = ? LIMIT 1"));
            cls.bindValue(0, fileId);
            cls.exec();
            if (cls.next())
                fallbackClass = cls.value(0).toString();
        }

        for (const auto &tc : cases) {
            ins.bindValue(0, fileId);
            ins.bindValue(1, tc.className.isEmpty() ? fallbackClass : tc.className);
            ins.bindValue(2, tc.methodName);
            ins.bindValue(3, tc.lineNum);
            ins.exec();
            ++count;
        }
    }

    m_stats.testCases = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 11: Scan CMake targets
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanCMakeTargets(QSqlDatabase &db)
{
    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO cmake_targets "
        "(file_id, target_name, target_type, line_num) VALUES (?,?,?,?)"));

    const QDir root(m_workspaceRoot);

    QDirIterator it(m_workspaceRoot,
                    QStringList{QStringLiteral("CMakeLists.txt"),
                                QStringLiteral("*.cmake")},
                    QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        const QString relDir = root.relativeFilePath(fi.absolutePath())
                                   .replace(QLatin1Char('\\'), QLatin1Char('/'));
        bool skip = false;
        for (const auto &part : relDir.split(QLatin1Char('/'))) {
            if (shouldSkipDir(part)) { skip = true; break; }
        }
        if (skip) continue;

        const QString content = readFileContent(fi.absoluteFilePath());
        if (content.isNull()) continue;

        const QString relPath = root.relativeFilePath(fi.absoluteFilePath())
                                    .replace(QLatin1Char('\\'), QLatin1Char('/'));

        QSqlQuery lookup(db);
        lookup.prepare(QStringLiteral(
            "SELECT id FROM files WHERE path = ?"));
        lookup.bindValue(0, relPath);
        lookup.exec();
        const QVariant fileId = lookup.next()
            ? QVariant(lookup.value(0).toLongLong())
            : QVariant();

        auto execIt = RE_CMAKE_EXEC.globalMatch(content);
        while (execIt.hasNext()) {
            auto m = execIt.next();
            const int lineNum = content.left(m.capturedStart())
                                    .count(QLatin1Char('\n')) + 1;
            ins.bindValue(0, fileId);
            ins.bindValue(1, m.captured(1));
            ins.bindValue(2, QStringLiteral("executable"));
            ins.bindValue(3, lineNum);
            ins.exec();
            ++count;
        }

        auto libIt = RE_CMAKE_LIB.globalMatch(content);
        while (libIt.hasNext()) {
            auto m = libIt.next();
            const int lineNum = content.left(m.capturedStart())
                                    .count(QLatin1Char('\n')) + 1;
            const QString context = content.mid(m.capturedStart(),
                                                qMin(100, content.size() - m.capturedStart()));
            const QString ttype = context.contains(QLatin1String("MODULE"))
                ? QStringLiteral("plugin") : QStringLiteral("library");
            ins.bindValue(0, fileId);
            ins.bindValue(1, m.captured(1));
            ins.bindValue(2, ttype);
            ins.bindValue(3, lineNum);
            ins.exec();
            ++count;
        }

        auto testIt = RE_CMAKE_TEST.globalMatch(content);
        while (testIt.hasNext()) {
            auto m = testIt.next();
            const int lineNum = content.left(m.capturedStart())
                                    .count(QLatin1Char('\n')) + 1;
            ins.bindValue(0, fileId);
            ins.bindValue(1, m.captured(1));
            ins.bindValue(2, QStringLiteral("test"));
            ins.bindValue(3, lineNum);
            ins.exec();
            ++count;
        }
    }

    m_stats.cmakeTargets = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 12: Build FTS5 full-text search index
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildFtsIndex(QSqlDatabase &db, bool hasFts5)
{
    if (!hasFts5) return 0;

    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO fts_index (name, qualified_name, file_path, kind, summary) "
        "VALUES (?,?,?,?,?)"));

    // Index files with summaries
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT f.path, fs.summary, fs.category "
            "FROM files f JOIN file_summaries fs ON f.id = fs.file_id"));
        while (q.next()) {
            const QString path = q.value(0).toString();
            const QFileInfo fi(path);
            ins.bindValue(0, fi.baseName());
            ins.bindValue(1, path);
            ins.bindValue(2, path);
            ins.bindValue(3, QStringLiteral("file"));
            ins.bindValue(4, q.value(1));
            ins.exec();
            ++count;
        }
    }

    // Index classes
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT c.name, c.bases, f.path "
            "FROM classes c JOIN files f ON c.file_id = f.id"));
        while (q.next()) {
            const QString bases = q.value(1).toString();
            ins.bindValue(0, q.value(0));
            ins.bindValue(1, q.value(0));
            ins.bindValue(2, q.value(2));
            ins.bindValue(3, QStringLiteral("class"));
            ins.bindValue(4, bases.isEmpty() ? QString()
                : QStringLiteral("bases: ") + bases);
            ins.exec();
            ++count;
        }
    }

    // Index functions
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT fi.name, fi.qualified_name, f.path "
            "FROM function_index fi JOIN files f ON fi.file_id = f.id"));
        while (q.next()) {
            ins.bindValue(0, q.value(0));
            ins.bindValue(1, q.value(1));
            ins.bindValue(2, q.value(2));
            ins.bindValue(3, QStringLiteral("function"));
            ins.bindValue(4, QString());
            ins.exec();
            ++count;
        }
    }

    // Index services
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT s.service_key, s.reg_type, f.path "
            "FROM services s JOIN files f ON s.file_id = f.id"));
        while (q.next()) {
            ins.bindValue(0, q.value(0));
            ins.bindValue(1, q.value(0));
            ins.bindValue(2, q.value(2));
            ins.bindValue(3, QStringLiteral("service"));
            ins.bindValue(4, q.value(1));
            ins.exec();
            ++count;
        }
    }

    // Index test cases
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT tc.test_method, tc.test_class, f.path "
            "FROM test_cases tc JOIN files f ON tc.file_id = f.id"));
        while (q.next()) {
            ins.bindValue(0, q.value(0));
            ins.bindValue(1, q.value(1).toString() + QLatin1Char('.') +
                             q.value(0).toString());
            ins.bindValue(2, q.value(2));
            ins.bindValue(3, QStringLiteral("test"));
            ins.bindValue(4, QString());
            ins.exec();
            ++count;
        }
    }

    m_stats.ftsEntries = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Full rebuild (all 12 steps)
// ══════════════════════════════════════════════════════════════════════════════

CodeGraphStats CodeGraphIndexer::fullRebuild(QSqlDatabase &db, bool hasFts5)
{
    m_stats = {};
    db.transaction();

    bool ok = true;
    try {
        scanFiles(db);
        parseAll(db);
        buildImplMap(db);
        buildSubsystemGraph(db);
        buildFunctionIndex(db);
        buildFileSummaries(db);
        buildEdges(db);
        scanQtConnections(db);
        scanServices(db);
        scanTestCases(db);
        scanCMakeTargets(db);
        buildFtsIndex(db, hasFts5);
    } catch (...) {
        ok = false;
    }

    if (ok)
        db.commit();
    else
        db.rollback();
    return m_stats;
}
