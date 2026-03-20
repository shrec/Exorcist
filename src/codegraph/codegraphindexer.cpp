#include "codegraphindexer.h"
#include "ilanguageindexer.h"
#include "cpplanguageindexer.h"
#include "tslanguageindexer.h"
#include "pylanguageindexer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QQueue>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QVariant>
#include <QXmlStreamReader>

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

static void insertSemanticTag(QSqlDatabase &db,
                              qint64 fileId,
                              const QString &entityType,
                              const QString &entityName,
                              int lineNum,
                              const QString &tag,
                              const QString &tagValue,
                              int confidence,
                              const QString &source,
                              const QString &evidence)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO semantic_tags "
        "(file_id, entity_type, entity_name, line_num, tag, tag_value, confidence, source, evidence) "
        "VALUES (?,?,?,?,?,?,?,?,?)"));
    q.bindValue(0, fileId);
    q.bindValue(1, entityType);
    q.bindValue(2, entityName);
    q.bindValue(3, lineNum);
    q.bindValue(4, tag);
    q.bindValue(5, tagValue);
    q.bindValue(6, confidence);
    q.bindValue(7, source);
    q.bindValue(8, evidence);
    q.exec();
}

static QStringList inferSemanticTags(const QString &path,
                                     const QString &lang,
                                     const QString &subsystem,
                                     const QString &content,
                                     const QString &entityName = {})
{
    const QString lowerPath = path.toLower();
    const QString lowerContent = content.toLower();
    const QString lowerName = entityName.toLower();
    QStringList tags;

    tags << QStringLiteral("lang:%1").arg(lang);
    tags << QStringLiteral("layer:semantic");
    if (!subsystem.isEmpty())
        tags << QStringLiteral("subsystem:%1").arg(subsystem);

    auto addIf = [&](bool cond, const char *value) {
        if (cond)
            tags << QString::fromLatin1(value);
    };

    addIf(lowerPath.contains(QStringLiteral("/plugin")) || subsystem.startsWith(QStringLiteral("plugin-")), "semantic:plugin");
    addIf(lowerPath.contains(QStringLiteral("/agent/")) || lowerName.contains(QStringLiteral("agent")), "semantic:agent");
    addIf(lowerPath.contains(QStringLiteral("/build/")) || lowerName.contains(QStringLiteral("build")), "semantic:build");
    addIf(lowerPath.contains(QStringLiteral("/debug/")) || lowerName.contains(QStringLiteral("debug")), "semantic:debug");
    addIf(lowerPath.contains(QStringLiteral("/test")) || lowerName.startsWith(QStringLiteral("test")), "semantic:test");
    addIf(lowerPath.contains(QStringLiteral("/search/")) || lowerName.contains(QStringLiteral("search")), "semantic:search");
    addIf(lowerPath.contains(QStringLiteral("/git/")) || lowerName.contains(QStringLiteral("git")), "semantic:git");
    addIf(lowerPath.contains(QStringLiteral("/terminal/")) || lowerName.contains(QStringLiteral("terminal")), "semantic:terminal");
    addIf(lowerPath.contains(QStringLiteral("/project/")) || lowerName.contains(QStringLiteral("project")), "semantic:project");
    addIf(lowerPath.contains(QStringLiteral("/profile/")) || lowerName.contains(QStringLiteral("profile")), "semantic:profile");
    addIf(lowerPath.contains(QStringLiteral("/dock/")) || lowerName.contains(QStringLiteral("dock")), "semantic:docking");
    addIf(lowerPath.contains(QStringLiteral("/ui/")) || lowerName.contains(QStringLiteral("widget")) ||
          lowerName.contains(QStringLiteral("panel")) || lowerName.contains(QStringLiteral("dialog")), "semantic:ui");
    addIf(lowerPath.contains(QStringLiteral("/codegraph/")) || lowerName.contains(QStringLiteral("index")) ||
          lowerName.contains(QStringLiteral("graph")) || lowerName.contains(QStringLiteral("query")), "semantic:analysis");
    addIf(lowerContent.contains(QStringLiteral("registerservice")) || lowerContent.contains(QStringLiteral("serviceregistry")), "semantic:service-registry");
    addIf(lowerContent.contains(QStringLiteral("q_object")) || lowerContent.contains(QStringLiteral("signals:")) ||
          lowerContent.contains(QStringLiteral("slots:")), "semantic:qt");
    addIf(lowerContent.contains(QStringLiteral("qprocess")) || lowerContent.contains(QStringLiteral("start(")), "semantic:process");
    addIf(lowerContent.contains(QStringLiteral("command")) || lowerName.contains(QStringLiteral("command")), "semantic:command");
    addIf(lowerContent.contains(QStringLiteral("network")) || lowerContent.contains(QStringLiteral("qnetwork")), "security:network-surface");
    addIf(lowerContent.contains(QStringLiteral("auth")) || lowerContent.contains(QStringLiteral("token")), "security:auth-surface");
    addIf(lowerContent.contains(QStringLiteral("permission")) || lowerContent.contains(QStringLiteral("secure")) ||
          lowerContent.contains(QStringLiteral("security")), "security:sensitive");
    addIf(lowerContent.contains(QStringLiteral("async")) || lowerContent.contains(QStringLiteral("qtconcurrent")) ||
          lowerContent.contains(QStringLiteral("qtimer::singleshot")), "performance:async");

    // Exorcist-native architecture tags
    addIf(lowerPath == QStringLiteral("src/mainwindow.cpp") || lowerPath == QStringLiteral("src/mainwindow.h"),
          "architecture:shell");
    addIf(lowerPath.contains(QStringLiteral("/bootstrap/")), "architecture:bootstrap");
    addIf(lowerPath.contains(QStringLiteral("/sdk/")) || lowerName.contains(QStringLiteral("ihostservices")) ||
          lowerName.startsWith(QLatin1Char('i')), "architecture:sdk-boundary");
    addIf(lowerPath.contains(QStringLiteral("/core/")), "architecture:core-interface");
    addIf(lowerPath.contains(QStringLiteral("/component/")) || lowerContent.contains(QStringLiteral("componentregistry")) ||
          lowerName.contains(QStringLiteral("component")), "architecture:shared-component");
    addIf(lowerPath.contains(QStringLiteral("/settings/")) || lowerPath.contains(QStringLiteral("/profile/")) ||
          lowerPath.contains(QStringLiteral("sharedservicesbootstrap")), "architecture:shared-service");
    addIf(lowerPath.contains(QStringLiteral("/plugin/")) && lowerName.contains(QStringLiteral("workbenchpluginbase")),
          "architecture:base-plugin");
    addIf(subsystem.startsWith(QStringLiteral("plugin-")) || lowerPath.startsWith(QStringLiteral("plugins/")),
          "architecture:plugin-domain");
    addIf(lowerContent.contains(QStringLiteral("workbenchpluginbase")) ||
          lowerName.contains(QStringLiteral("plugin")), "architecture:plugin-owned");
    addIf(lowerContent.contains(QStringLiteral("idockmanager")) ||
          lowerContent.contains(QStringLiteral("imenumanager")) ||
          lowerContent.contains(QStringLiteral("itoolbarmanager")) ||
          lowerContent.contains(QStringLiteral("istatusbarmanager")),
          "ui:workbench-surface");
    addIf(lowerContent.contains(QStringLiteral("addmenucommand")) ||
          lowerContent.contains(QStringLiteral("createmenu")) ||
          lowerContent.contains(QStringLiteral("imenumanager")),
          "ui:menu-owner");
    addIf(lowerContent.contains(QStringLiteral("createtoolbar")) ||
          lowerContent.contains(QStringLiteral("itoolbarmanager")),
          "ui:toolbar-owner");
    addIf(lowerContent.contains(QStringLiteral("dock(")) ||
          lowerContent.contains(QStringLiteral("showdock")) ||
          lowerContent.contains(QStringLiteral("idockmanager")),
          "ui:dock-owner");
    addIf(lowerContent.contains(QStringLiteral("iprofilemanager")) ||
          lowerContent.contains(QStringLiteral("loadprofilesfromdirectory")) ||
          lowerName.contains(QStringLiteral("profilemanager")),
          "workspace:profile-activation");
    addIf(lowerName.contains(QStringLiteral("projecttemplate")) ||
          lowerContent.contains(QStringLiteral("projecttemplateregistry")),
          "workspace:template-system");

    tags.removeDuplicates();
    return tags;
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
// Step 13: Build function-level call graph
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildCallGraph(QSqlDatabase &db)
{
    // Map: function name → its file_id (first occurrence wins for edge target)
    QHash<QString, qint64> knownFuncFile;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT name, file_id FROM function_index GROUP BY name"));
        while (q.next())
            knownFuncFile.insert(q.value(0).toString(), q.value(1).toLongLong());
    }

    static const QRegularExpression RE_CALL(
        QStringLiteral(R"(\b([A-Za-z_]\w{2,})\s*\()"));

    static const QSet<QString> CALL_SKIP = {
        QStringLiteral("if"),    QStringLiteral("for"),   QStringLiteral("while"),
        QStringLiteral("switch"),QStringLiteral("return"),QStringLiteral("catch"),
        QStringLiteral("sizeof"),QStringLiteral("new"),   QStringLiteral("delete"),
        QStringLiteral("throw"), QStringLiteral("static_cast"),
        QStringLiteral("const_cast"), QStringLiteral("dynamic_cast"),
        QStringLiteral("reinterpret_cast"),
    };

    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO call_graph "
        "(caller_file_id, caller_func, callee_func, callee_file_id, line_num) "
        "VALUES (?,?,?,?,?)"));

    // Iterate C++ source files that have indexed functions
    QSqlQuery fileQ(db);
    fileQ.exec(QStringLiteral(
        "SELECT DISTINCT f.id, f.path "
        "FROM function_index fi JOIN files f ON fi.file_id = f.id "
        "WHERE f.lang = 'cpp' AND f.ext = '.cpp'"));

    while (fileQ.next()) {
        const qint64 fileId = fileQ.value(0).toLongLong();
        const QString path  = fileQ.value(1).toString();

        const QString content = readFileContent(m_workspaceRoot + QLatin1Char('/') + path);
        if (content.isNull()) continue;
        const QStringList lines = content.split(QLatin1Char('\n'));

        QSqlQuery funcQ(db);
        funcQ.prepare(QStringLiteral(
            "SELECT name, start_line, end_line FROM function_index WHERE file_id = ?"));
        funcQ.bindValue(0, fileId);
        funcQ.exec();

        while (funcQ.next()) {
            const QString callerName = funcQ.value(0).toString();
            const int startLine = qMax(0, funcQ.value(1).toInt() - 1);
            const int endLine   = qMin(lines.size(), funcQ.value(2).toInt());

            QSet<QString> seen;
            for (int li = startLine; li < endLine; ++li) {
                const QString &line = lines[li];
                if (line.trimmed().startsWith(QLatin1String("//")))
                    continue;

                auto callIt = RE_CALL.globalMatch(line);
                while (callIt.hasNext()) {
                    auto m = callIt.next();
                    const QString callee = m.captured(1);
                    if (CALL_SKIP.contains(callee) || callee == callerName)
                        continue;
                    if (!knownFuncFile.contains(callee))
                        continue;
                    if (seen.contains(callee))
                        continue;
                    seen.insert(callee);

                    ins.bindValue(0, fileId);
                    ins.bindValue(1, callerName);
                    ins.bindValue(2, callee);
                    ins.bindValue(3, knownFuncFile.value(callee));
                    ins.bindValue(4, li + 1);
                    if (ins.exec()) ++count;
                }
            }
        }
    }

    m_stats.callGraphEdges = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 14: Scan XML bindings and validate configs
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanXmlBindings(QSqlDatabase &db)
{
    // Known symbols for binding resolution
    QSet<QString> knownSymbols;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "SELECT name FROM classes UNION SELECT name FROM function_index"));
        while (q.next())
            knownSymbols.insert(q.value(0).toString());
    }

    static const QSet<QString> BINDING_ATTRS = {
        QStringLiteral("class"), QStringLiteral("type"), QStringLiteral("base"),
        QStringLiteral("slot"),  QStringLiteral("signal"), QStringLiteral("name"),
    };

    int bindCount = 0, issueCount = 0;
    QSqlQuery insBinding(db);
    insBinding.prepare(QStringLiteral(
        "INSERT INTO xml_bindings "
        "(xml_file_id, element, attribute, value, bound_symbol, bound_file_id, binding_type) "
        "VALUES (?,?,?,?,?,?,?)"));

    QSqlQuery insIssue(db);
    insIssue.prepare(QStringLiteral(
        "INSERT INTO config_issues "
        "(xml_file_id, issue_type, element, attribute, value, message, severity) "
        "VALUES (?,?,?,?,?,?,?)"));

    QSqlQuery xmlFiles(db);
    xmlFiles.exec(QStringLiteral(
        "SELECT id, path FROM files WHERE ext IN ('.xml', '.ui', '.qrc')"));

    while (xmlFiles.next()) {
        const qint64 fileId = xmlFiles.value(0).toLongLong();
        const QString path  = xmlFiles.value(1).toString();
        const QString absPath = m_workspaceRoot + QLatin1Char('/') + path;

        QFile f(absPath);
        if (!f.open(QIODevice::ReadOnly)) continue;

        QXmlStreamReader xml(&f);
        QHash<QString, int> idCount;

        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) continue;

            const QString elemName = xml.name().toString();
            const auto attrs = xml.attributes();

            for (const QXmlStreamAttribute &attr : attrs) {
                const QString attrName = attr.name().toString();
                const QString attrVal  = attr.value().toString();

                // Duplicate id/name detection
                if (attrName == QLatin1String("id") ||
                    attrName == QLatin1String("name")) {
                    if (++idCount[attrVal] > 1) {
                        insIssue.bindValue(0, fileId);
                        insIssue.bindValue(1, QStringLiteral("duplicate_id"));
                        insIssue.bindValue(2, elemName);
                        insIssue.bindValue(3, attrName);
                        insIssue.bindValue(4, attrVal);
                        insIssue.bindValue(5,
                            QStringLiteral("Duplicate %1 '%2'").arg(attrName, attrVal));
                        insIssue.bindValue(6, QStringLiteral("warning"));
                        insIssue.exec();
                        ++issueCount;
                    }
                }

                // Symbol binding check
                if (BINDING_ATTRS.contains(attrName) && !attrVal.isEmpty()) {
                    const bool bound = knownSymbols.contains(attrVal);
                    insBinding.bindValue(0, fileId);
                    insBinding.bindValue(1, elemName);
                    insBinding.bindValue(2, attrName);
                    insBinding.bindValue(3, attrVal);
                    insBinding.bindValue(4, bound ? attrVal : QString());
                    insBinding.bindValue(5, QVariant(QVariant::Invalid));
                    insBinding.bindValue(6, bound
                        ? QStringLiteral("resolved")
                        : QStringLiteral("unresolved"));
                    insBinding.exec();
                    ++bindCount;
                }
            }
        }
    }

    m_stats.xmlBindings  = bindCount;
    m_stats.configIssues = issueCount;
    return bindCount + issueCount;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 15: Scan runtime loading entrypoints
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::scanRuntimeEntrypoints(QSqlDatabase &db)
{
    static const QRegularExpression RE_QPLUGINLOADER(
        QStringLiteral(R"(QPluginLoader\s*\(\s*([^)]+)\))"));
    static const QRegularExpression RE_QLIBRARY(
        QStringLiteral(R"re(QLibrary\s*\(\s*"([^"]+)")re"));
    static const QRegularExpression RE_LOAD_SCRIPT(
        QStringLiteral(R"re(load(?:Lua|Script|File)\s*\(\s*"([^"]+)")re"));
    static const QRegularExpression RE_DLOPEN(
        QStringLiteral(R"re(dlopen\s*\(\s*"([^"]+)")re"));

    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO runtime_entrypoints "
        "(file_id, loader_type, target, line_num) VALUES (?,?,?,?)"));

    QSqlQuery sel(db);
    sel.exec(QStringLiteral(
        "SELECT id, path FROM files WHERE ext = '.cpp' AND lang = 'cpp'"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();

        const QString content = readFileContent(m_workspaceRoot + QLatin1Char('/') + path);
        if (content.isNull()) continue;

        struct Pattern { const QRegularExpression *re; QString loaderType; };
        const Pattern patterns[] = {
            {&RE_QPLUGINLOADER, QStringLiteral("QPluginLoader")},
            {&RE_QLIBRARY,      QStringLiteral("QLibrary")},
            {&RE_LOAD_SCRIPT,   QStringLiteral("script")},
            {&RE_DLOPEN,        QStringLiteral("dlopen")},
        };

        for (const auto &p : patterns) {
            auto it = p.re->globalMatch(content);
            while (it.hasNext()) {
                auto m = it.next();
                const int lineNum =
                    content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
                QString target = m.captured(1).trimmed();
                if (target.startsWith(QLatin1Char('"')))
                    target = target.mid(1, target.size() - 2);

                ins.bindValue(0, fileId);
                ins.bindValue(1, p.loaderType);
                ins.bindValue(2, target);
                ins.bindValue(3, lineNum);
                if (ins.exec()) ++count;
            }
        }
    }

    m_stats.runtimeEntrypoints = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 16: Build symbol alias map (typo detection)
// ══════════════════════════════════════════════════════════════════════════════

static int s_levenshtein(const QString &a, const QString &b)
{
    const int m = a.size(), n = b.size();
    if (m == 0) return n;
    if (n == 0) return m;
    if (qAbs(m - n) > 4) return 99; // fast reject: length difference too large

    QVector<int> prev(n + 1), curr(n + 1);
    for (int j = 0; j <= n; ++j) prev[j] = j;

    for (int i = 1; i <= m; ++i) {
        curr[0] = i;
        for (int j = 1; j <= n; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = qMin(prev[j] + 1,
                      qMin(curr[j - 1] + 1, prev[j - 1] + cost));
        }
        prev = curr;
    }
    return prev[n];
}

int CodeGraphIndexer::buildSymbolAliases(QSqlDatabase &db)
{
    struct Symbol { QString name; qint64 fileId; };
    QHash<QString, QVector<Symbol>> byPrefix; // first-3-char prefix → symbols

    auto addSymbols = [&](const QString &queryStr) {
        QSqlQuery q(db);
        q.exec(queryStr);
        while (q.next()) {
            const QString name  = q.value(0).toString();
            const qint64 fileId = q.value(1).toLongLong();
            if (name.size() < 4) continue;
            byPrefix[name.left(3).toLower()].append({name, fileId});
        }
    };
    addSymbols(QStringLiteral("SELECT name, file_id FROM classes"));
    addSymbols(QStringLiteral("SELECT DISTINCT name, file_id FROM function_index"));

    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO symbol_aliases "
        "(symbol_a, file_a, symbol_b, file_b, edit_distance, alias_type) "
        "VALUES (?,?,?,?,?,?)"));

    for (auto groupIt = byPrefix.constBegin(); groupIt != byPrefix.constEnd(); ++groupIt) {
        const auto &group = groupIt.value();
        if (group.size() < 2 || group.size() > 200) continue;

        for (int i = 0; i < group.size(); ++i) {
            for (int j = i + 1; j < group.size(); ++j) {
                const auto &sa = group[i];
                const auto &sb = group[j];
                if (sa.name == sb.name) continue;

                const int dist = s_levenshtein(sa.name, sb.name);
                if (dist > 3) continue;

                ins.bindValue(0, sa.name);
                ins.bindValue(1, sa.fileId);
                ins.bindValue(2, sb.name);
                ins.bindValue(3, sb.fileId);
                ins.bindValue(4, dist);
                ins.bindValue(5, dist <= 2
                    ? QStringLiteral("typo")
                    : QStringLiteral("alias"));
                if (ins.exec()) ++count;
            }
        }
    }

    m_stats.symbolAliases = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 17: Build reachability graph (BFS from entry points)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildReachability(QSqlDatabase &db)
{
    static const QSet<QString> ENTRY_NAMES = {
        QStringLiteral("main"),    QStringLiteral("initialize"),
        QStringLiteral("shutdown"),QStringLiteral("initPlugin"),
        QStringLiteral("create"),  QStringLiteral("instance"),
        QStringLiteral("load"),    QStringLiteral("unload"),
        QStringLiteral("start"),   QStringLiteral("stop"),
        QStringLiteral("run"),     QStringLiteral("exec"),
        QStringLiteral("init"),    QStringLiteral("setup"),
    };

    struct FuncInfo { qint64 fileId; bool implicit; };
    QHash<QString, FuncInfo> allFuncs;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT name, file_id FROM function_index"));
        while (q.next()) {
            const QString nm = q.value(0).toString();
            // Slots (on*), test methods, signals are implicitly reachable via Qt
            const bool implicit =
                nm.startsWith(QLatin1String("on")) ||
                nm.startsWith(QLatin1String("test")) ||
                nm.startsWith(QLatin1String("Test"));
            allFuncs.insert(nm, {q.value(1).toLongLong(), implicit});
        }
    }

    // Build call-graph adjacency: caller → callees
    QHash<QString, QVector<QString>> calls;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT caller_func, callee_func FROM call_graph"));
        while (q.next())
            calls[q.value(0).toString()].append(q.value(1).toString());
    }

    // BFS
    QSet<QString> reachable;
    QQueue<QString> queue;
    for (auto it = allFuncs.constBegin(); it != allFuncs.constEnd(); ++it) {
        const QString &name = it.key();
        if (ENTRY_NAMES.contains(name) || it.value().implicit) {
            if (!reachable.contains(name)) {
                reachable.insert(name);
                queue.enqueue(name);
            }
        }
    }
    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        for (const QString &callee : calls.value(current)) {
            if (!reachable.contains(callee) && allFuncs.contains(callee)) {
                reachable.insert(callee);
                queue.enqueue(callee);
            }
        }
    }

    // Write results
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO reachability "
        "(file_id, symbol, symbol_type, is_reachable, reachable_via, dead_reason) "
        "VALUES (?,?,?,?,?,?)"));

    int reachCount = 0, deadCount = 0;
    for (auto it = allFuncs.constBegin(); it != allFuncs.constEnd(); ++it) {
        const bool reach = reachable.contains(it.key());
        ins.bindValue(0, it.value().fileId);
        ins.bindValue(1, it.key());
        ins.bindValue(2, QStringLiteral("function"));
        ins.bindValue(3, reach ? 1 : 0);
        ins.bindValue(4, reach ? QStringLiteral("call_graph") : QString());
        ins.bindValue(5, reach ? QString() : QStringLiteral("no_call_path"));
        ins.exec();
        if (reach) ++reachCount; else ++deadCount;
    }

    m_stats.reachableFunctions = reachCount;
    m_stats.deadFunctions      = deadCount;
    return reachCount + deadCount;
}

// ══════════════════════════════════════════════════════════════════════════════
// Step 18: Score hotspot files (coupling × coverage × crash-risk)
// ══════════════════════════════════════════════════════════════════════════════

int CodeGraphIndexer::buildHotspotScores(QSqlDatabase &db)
{
    static const QRegularExpression RE_BARE_NEW(
        QStringLiteral(R"(\bnew[\s(])"));
    static const QRegularExpression RE_BARE_DELETE(
        QStringLiteral(R"(\bdelete[\s\[])"));
    static const QRegularExpression RE_PTR_CAST(
        QStringLiteral(R"(reinterpret_cast|static_cast<\w+\*>)"));

    auto countRe = [](const QRegularExpression &re, const QString &text) -> int {
        int n = 0;
        auto it = re.globalMatch(text);
        while (it.hasNext()) { it.next(); ++n; }
        return n;
    };

    int count = 0;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO hotspot_scores "
        "(file_id, coupling_in, coupling_out, has_tests, "
        "crash_indicators, hotspot_score, risk_factors) "
        "VALUES (?,?,?,?,?,?,?)"));

    QSqlQuery sel(db);
    sel.exec(QStringLiteral(
        "SELECT id, path FROM files "
        "WHERE ext = '.cpp' AND lang = 'cpp' AND path LIKE 'src/%'"));

    while (sel.next()) {
        const qint64 fileId = sel.value(0).toLongLong();
        const QString path  = sel.value(1).toString();

        // Coupling
        int couplingIn = 0, couplingOut = 0;
        {
            QSqlQuery eq(db);
            eq.prepare(QStringLiteral(
                "SELECT "
                "  (SELECT COUNT(*) FROM edges WHERE target_file=? AND edge_type='includes'),"
                "  (SELECT COUNT(*) FROM edges WHERE source_file=? AND edge_type='includes')"));
            eq.bindValue(0, fileId);
            eq.bindValue(1, fileId);
            eq.exec();
            if (eq.next()) {
                couplingIn  = eq.value(0).toInt();
                couplingOut = eq.value(1).toInt();
            }
        }

        // Has tests?
        int hasTests = 0;
        {
            QSqlQuery tq(db);
            tq.prepare(QStringLiteral(
                "SELECT 1 FROM edges WHERE target_file=? AND edge_type='tests' LIMIT 1"));
            tq.bindValue(0, fileId);
            tq.exec();
            hasTests = tq.next() ? 1 : 0;
        }

        // Crash indicators from file content
        const QString content = readFileContent(m_workspaceRoot + QLatin1Char('/') + path);
        if (content.isNull()) continue;

        const int arrowCount = content.count(QLatin1String("->"));
        const int bareNew    = countRe(RE_BARE_NEW,    content);
        const int bareDel    = countRe(RE_BARE_DELETE, content);
        const int ptrCast    = countRe(RE_PTR_CAST,    content);
        const int crashTotal = arrowCount + bareNew * 3 + bareDel * 3 + ptrCast * 2;

        const double couplingNorm = qMin(1.0, (couplingIn + couplingOut) / 50.0);
        const double coverageGap  = hasTests ? 0.0 : 1.0;
        const double crashNorm    = qMin(1.0, crashTotal / 1000.0);
        const double score =
            (couplingNorm * 0.35 + coverageGap * 0.40 + crashNorm * 0.25) * 100.0;

        QStringList risks;
        if (arrowCount > 100)
            risks << QStringLiteral("heavy-deref:%1").arg(arrowCount);
        if (bareNew > 0)
            risks << QStringLiteral("bare-new:%1").arg(bareNew);
        if (bareDel > 0)
            risks << QStringLiteral("bare-delete:%1").arg(bareDel);
        if (ptrCast > 0)
            risks << QStringLiteral("ptr-cast:%1").arg(ptrCast);

        ins.bindValue(0, fileId);
        ins.bindValue(1, couplingIn);
        ins.bindValue(2, couplingOut);
        ins.bindValue(3, hasTests);
        ins.bindValue(4, crashTotal);
        ins.bindValue(5, score);
        ins.bindValue(6, risks.join(QStringLiteral(", ")));
        if (ins.exec()) ++count;
    }

    m_stats.hotspotFiles = count;
    return count;
}

int CodeGraphIndexer::buildSemanticTags(QSqlDatabase &db)
{
    int count = 0;

    QSqlQuery files(db);
    files.exec(QStringLiteral("SELECT id, path, lang, subsystem FROM files"));
    while (files.next()) {
        const qint64 fileId = files.value(0).toLongLong();
        const QString path = files.value(1).toString();
        const QString lang = files.value(2).toString();
        const QString subsystem = files.value(3).toString();
        const QString content = readFileContent(m_workspaceRoot + QLatin1Char('/') + path);

        const QStringList tags = inferSemanticTags(path, lang, subsystem, content, QFileInfo(path).fileName());
        for (const auto &tagValue : tags) {
            const int sep = tagValue.indexOf(QLatin1Char(':'));
            const QString tag = sep > 0 ? tagValue.left(sep) : tagValue;
            const QString value = sep > 0 ? tagValue.mid(sep + 1) : QString();
            insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                              tag, value, 85, QStringLiteral("heuristic"), path);
            ++count;
        }

        const bool isProjectOwned = path.startsWith(QStringLiteral("src/")) ||
                                    path.startsWith(QStringLiteral("plugins/")) ||
                                    path.startsWith(QStringLiteral("server/"));

        if (isProjectOwned) {
            QSqlQuery metrics(db);
            metrics.prepare(QStringLiteral(
                "SELECT coupling_in, coupling_out, has_tests, crash_indicators, hotspot_score "
                "FROM hotspot_scores WHERE file_id = ?"));
            metrics.bindValue(0, fileId);
            metrics.exec();
            if (metrics.next()) {
                const bool hasTests = metrics.value(2).toInt() != 0;
                const int crashIndicators = metrics.value(3).toInt();
                const double hotspotScore = metrics.value(4).toDouble();

                insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                                  QStringLiteral("audit"),
                                  hasTests ? QStringLiteral("unit-covered") : QStringLiteral("uncovered"),
                                  90, QStringLiteral("derived"), QStringLiteral("hotspot_scores"));
                ++count;

                if (hotspotScore >= 8.0) {
                    insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                                      QStringLiteral("performance"), QStringLiteral("hot-path"),
                                      95, QStringLiteral("derived"), QStringLiteral("hotspot_scores"));
                    ++count;
                }
                if (crashIndicators > 0) {
                    insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                                      QStringLiteral("security"), QStringLiteral("fragile"),
                                      80, QStringLiteral("derived"), QStringLiteral("hotspot_scores"));
                    ++count;
                }
            }

            QSqlQuery runtime(db);
            runtime.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM runtime_entrypoints WHERE file_id = ?"));
            runtime.bindValue(0, fileId);
            runtime.exec();
            if (runtime.next() && runtime.value(0).toInt() > 0) {
                insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                                  QStringLiteral("runtime"), QStringLiteral("entrypoint"),
                                  90, QStringLiteral("derived"), QStringLiteral("runtime_entrypoints"));
                ++count;
            }

            QSqlQuery services(db);
            services.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM services WHERE file_id = ?"));
            services.bindValue(0, fileId);
            services.exec();
            if (services.next() && services.value(0).toInt() > 0) {
                insertSemanticTag(db, fileId, QStringLiteral("file"), path, 0,
                                  QStringLiteral("architecture"), QStringLiteral("service-participant"),
                                  85, QStringLiteral("derived"), QStringLiteral("services"));
                ++count;
            }
        }

        QSqlQuery classes(db);
        classes.prepare(QStringLiteral(
            "SELECT name, line_num FROM classes WHERE file_id = ?"));
        classes.bindValue(0, fileId);
        classes.exec();
        while (classes.next()) {
            const QString name = classes.value(0).toString();
            const int lineNum = classes.value(1).toInt();
            const QStringList classTags = inferSemanticTags(path, lang, subsystem, QString(), name);
            for (const auto &tagValue : classTags) {
                const int sep = tagValue.indexOf(QLatin1Char(':'));
                const QString tag = sep > 0 ? tagValue.left(sep) : tagValue;
                const QString value = sep > 0 ? tagValue.mid(sep + 1) : QString();
                insertSemanticTag(db, fileId, QStringLiteral("class"), name, lineNum,
                                  tag, value, 75, QStringLiteral("heuristic"), name);
                ++count;
            }
        }

        QSqlQuery funcs(db);
        funcs.prepare(QStringLiteral(
            "SELECT qualified_name, start_line FROM function_index WHERE file_id = ?"));
        funcs.bindValue(0, fileId);
        funcs.exec();
        while (funcs.next()) {
            const QString qualifiedName = funcs.value(0).toString();
            const int lineNum = funcs.value(1).toInt();
            const QStringList fnTags = inferSemanticTags(path, lang, subsystem, QString(), qualifiedName);
            for (const auto &tagValue : fnTags) {
                const int sep = tagValue.indexOf(QLatin1Char(':'));
                const QString tag = sep > 0 ? tagValue.left(sep) : tagValue;
                const QString value = sep > 0 ? tagValue.mid(sep + 1) : QString();
                insertSemanticTag(db, fileId, QStringLiteral("function"), qualifiedName, lineNum,
                                  tag, value, 65, QStringLiteral("heuristic"), qualifiedName);
                ++count;
            }
        }
    }

    m_stats.semanticTags = count;
    return count;
}

// ══════════════════════════════════════════════════════════════════════════════
// Full rebuild
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
        buildCallGraph(db);
        scanXmlBindings(db);
        scanRuntimeEntrypoints(db);
        buildSymbolAliases(db);
        buildReachability(db);
        buildHotspotScores(db);
        buildSemanticTags(db);
    } catch (...) {
        ok = false;
    }

    if (ok)
        db.commit();
    else
        db.rollback();
    return m_stats;
}
