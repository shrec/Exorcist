#include "tslanguageindexer.h"
#include "codegraphutils.h"

#include <QRegularExpression>

// ══════════════════════════════════════════════════════════════════════════════
// TypeScript / JavaScript regex patterns
// ══════════════════════════════════════════════════════════════════════════════

static const QRegularExpression RE_TS_CLASS(
    QStringLiteral(R"(^\s*(?:export\s+)?class\s+(\w+))"
                   R"((?:\s+extends\s+(\w+))?)"
                   R"((?:\s+implements\s+([\w,\s]+?))?\s*\{)"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_TS_IMPORT(
    QStringLiteral(R"(^\s*import\s+.+?\s+from\s+['"]([^'"]+)['"])"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_TS_FUNC(
    QStringLiteral(R"(^\s*(?:export\s+)?(?:async\s+)?function\s+(\w+)\s*\()"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_TS_METHOD(
    QStringLiteral(R"(^\s+(?:public|private|protected|static|async|readonly|\s)*(\w+)\s*\()"),
    QRegularExpression::MultilineOption);

// Jest / Mocha test patterns
static const QRegularExpression RE_TS_TEST(
    QStringLiteral(R"((?:test|it)\s*\(\s*['"]([^'"]+)['"])"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_TS_DESCRIBE(
    QStringLiteral(R"(describe\s*\(\s*['"]([^'"]+)['"])"),
    QRegularExpression::MultilineOption);

// ══════════════════════════════════════════════════════════════════════════════

QString TsLanguageIndexer::languageId() const
{
    return QStringLiteral("typescript");
}

QStringList TsLanguageIndexer::fileExtensions() const
{
    return {
        QStringLiteral(".ts"),  QStringLiteral(".tsx"),
        QStringLiteral(".js"),  QStringLiteral(".mjs"),
    };
}

// ── File metadata ────────────────────────────────────────────────────────────

CodeGraphFileMetadata TsLanguageIndexer::extractFileMetadata(const QString & /*content*/) const
{
    return {};
}

// ── Structural parsing ──────────────────────────────────────────────────────

CodeGraphParseResult TsLanguageIndexer::parseFile(const QString &content) const
{
    CodeGraphParseResult result;

    // ── Imports ──
    auto importIt = RE_TS_IMPORT.globalMatch(content);
    while (importIt.hasNext()) {
        auto m = importIt.next();
        CodeGraphImportInfo imp;
        imp.path     = m.captured(1);
        imp.isSystem = false;
        result.imports.append(imp);
    }

    // ── Classes ──
    auto classIt = RE_TS_CLASS.globalMatch(content);
    while (classIt.hasNext()) {
        auto m = classIt.next();
        CodeGraphClassInfo cls;
        cls.name    = m.captured(1);
        cls.bases   = m.captured(2); // extends
        cls.lineNum = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        result.classes.append(cls);
    }

    return result;
}

// ── Function extraction ──────────────────────────────────────────────────────

QVector<CodeGraphFunction> TsLanguageIndexer::extractFunctions(const QString &content) const
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    QVector<CodeGraphFunction> functions;
    const int n = lines.size();

    for (int i = 0; i < n; ++i) {
        const QString &line = lines[i];
        const QString stripped = line.trimmed();

        // Skip comments
        if (stripped.isEmpty() ||
            stripped.startsWith(QLatin1String("//")) ||
            stripped.startsWith(QLatin1String("/*")))
            continue;

        // Top-level function
        auto fm = RE_TS_FUNC.match(stripped);
        if (fm.hasMatch()) {
            const int endIdx = CodeGraphUtils::findBraceBody(lines, i, n);
            if (endIdx >= 0) {
                CodeGraphFunction fn;
                fn.name          = fm.captured(1);
                fn.qualifiedName = fn.name;
                fn.isMethod      = false;
                fn.startLine     = i + 1;
                fn.endLine       = endIdx + 1;
                fn.lineCount     = endIdx - i + 1;
                functions.append(fn);
                i = endIdx;
                continue;
            }
        }
    }

    return functions;
}

// ── First comment extraction ─────────────────────────────────────────────────

QString TsLanguageIndexer::extractFirstComment(const QStringList &lines) const
{
    for (int li = 0; li < qMin(20, lines.size()); ++li) {
        const QString s = lines[li].trimmed();
        if (s.startsWith(QLatin1String("///"))) {
            QString comment = s;
            while (comment.startsWith(QLatin1Char('/')))
                comment = comment.mid(1);
            return comment.trimmed();
        }
        if (s.startsWith(QLatin1String("/**"))) {
            QString text = s.mid(3).trimmed();
            text.remove(QLatin1String("*/"));
            if (!text.isEmpty())
                return text;
            for (int nxt = li + 1; nxt < qMin(li + 5, lines.size()); ++nxt) {
                text = lines[nxt].trimmed();
                while (text.startsWith(QLatin1Char('*')))
                    text = text.mid(1).trimmed();
                if (!text.isEmpty() && text != QLatin1String("/"))
                    return text;
            }
            break;
        }
        if (s.startsWith(QLatin1String("import ")) ||
            s.startsWith(QLatin1String("\"use strict\"")))
            continue;
        if (!s.isEmpty() && !s.startsWith(QLatin1String("//")))
            break;
    }
    return {};
}

// ── Framework-specific scanning ──────────────────────────────────────────────

QVector<CodeGraphConnectionInfo> TsLanguageIndexer::scanConnections(const QString & /*content*/) const
{
    // JS/TS addEventListener could be detected here in the future.
    return {};
}

QVector<CodeGraphServiceRef> TsLanguageIndexer::scanServiceRefs(const QString & /*content*/) const
{
    return {};
}

QVector<CodeGraphTestCase> TsLanguageIndexer::scanTestCases(const QString &content) const
{
    QVector<CodeGraphTestCase> results;

    // Find describe block name (used as "class")
    QString describeName;
    auto dm = RE_TS_DESCRIBE.match(content);
    if (dm.hasMatch())
        describeName = dm.captured(1);

    // Find test/it calls
    auto testIt = RE_TS_TEST.globalMatch(content);
    while (testIt.hasNext()) {
        auto m = testIt.next();
        CodeGraphTestCase tc;
        tc.className  = describeName;
        tc.methodName = m.captured(1);
        tc.lineNum    = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        results.append(tc);
    }

    return results;
}
