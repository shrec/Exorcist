#include "pylanguageindexer.h"

#include <QRegularExpression>

// ══════════════════════════════════════════════════════════════════════════════
// Python regex patterns
// ══════════════════════════════════════════════════════════════════════════════

static const QRegularExpression RE_PY_CLASS(
    QStringLiteral(R"(^class\s+(\w+)(?:\s*\(\s*([\w., ]+)\s*\))?)"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_PY_DEF(
    QStringLiteral(R"(^(\s*)def\s+(\w+)\s*\()"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_PY_IMPORT(
    QStringLiteral(R"(^(?:from\s+([\w.]+)\s+)?import\s+([\w., ]+))"),
    QRegularExpression::MultilineOption);

// pytest: def test_xxx()
static const QRegularExpression RE_PY_TEST(
    QStringLiteral(R"(^\s*def\s+(test_\w+)\s*\()"),
    QRegularExpression::MultilineOption);

// ══════════════════════════════════════════════════════════════════════════════

QString PythonLanguageIndexer::languageId() const
{
    return QStringLiteral("python");
}

QStringList PythonLanguageIndexer::fileExtensions() const
{
    return { QStringLiteral(".py") };
}

// ── File metadata ────────────────────────────────────────────────────────────

CodeGraphFileMetadata PythonLanguageIndexer::extractFileMetadata(const QString & /*content*/) const
{
    return {};
}

// ── Structural parsing ──────────────────────────────────────────────────────

CodeGraphParseResult PythonLanguageIndexer::parseFile(const QString &content) const
{
    CodeGraphParseResult result;

    // ── Imports ──
    auto importIt = RE_PY_IMPORT.globalMatch(content);
    while (importIt.hasNext()) {
        auto m = importIt.next();
        CodeGraphImportInfo imp;
        imp.path = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        result.imports.append(imp);
    }

    // ── Classes ──
    auto classIt = RE_PY_CLASS.globalMatch(content);
    while (classIt.hasNext()) {
        auto m = classIt.next();
        CodeGraphClassInfo cls;
        cls.name    = m.captured(1);
        cls.bases   = m.captured(2).trimmed();
        cls.lineNum = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        result.classes.append(cls);
    }

    return result;
}

// ── Function extraction (indentation-based) ──────────────────────────────────

QVector<CodeGraphFunction> PythonLanguageIndexer::extractFunctions(const QString &content) const
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    QVector<CodeGraphFunction> functions;
    const int n = lines.size();
    QString currentClass;

    for (int i = 0; i < n; ++i) {
        const QString &line = lines[i];

        // Track current class
        auto cm = RE_PY_CLASS.match(line);
        if (cm.hasMatch()) {
            currentClass = cm.captured(1);
            continue;
        }

        // Function/method definition
        auto m = RE_PY_DEF.match(line);
        if (m.hasMatch()) {
            const int indent = m.captured(1).size();
            const QString funcName = m.captured(2);

            // Find function end (next line at same or lesser indent)
            int end = n;
            for (int j = i + 1; j < n; ++j) {
                const QString &nextLine = lines[j];
                if (!nextLine.trimmed().isEmpty()) {
                    const int nextIndent = nextLine.size() - nextLine.trimmed().size();
                    if (nextIndent <= indent) {
                        end = j;
                        break;
                    }
                }
            }

            const bool isMethod = indent > 0 && !currentClass.isEmpty();
            const QString cls = isMethod ? currentClass : QString();
            const QString qname = cls.isEmpty()
                ? funcName
                : cls + QLatin1Char('.') + funcName;

            CodeGraphFunction fn;
            fn.name          = funcName;
            fn.qualifiedName = qname;
            fn.className     = cls;
            fn.isMethod      = isMethod;
            fn.startLine     = i + 1;
            fn.endLine       = end;
            fn.lineCount     = end - i;
            functions.append(fn);
        }
    }

    return functions;
}

// ── First comment extraction ─────────────────────────────────────────────────

QString PythonLanguageIndexer::extractFirstComment(const QStringList &lines) const
{
    for (int li = 0; li < qMin(15, lines.size()); ++li) {
        const QString s = lines[li].trimmed();
        if (s.startsWith(QLatin1Char('#')) &&
            !s.startsWith(QLatin1String("#!"))) {
            QString comment = s;
            while (comment.startsWith(QLatin1Char('#')))
                comment = comment.mid(1);
            return comment.trimmed();
        }
        if (s.contains(QLatin1String("\"\"\"")) ||
            s.contains(QLatin1String("'''"))) {
            QString comment = s;
            comment.remove(QLatin1Char('"'));
            comment.remove(QLatin1Char('\''));
            return comment.trimmed();
        }
    }
    return {};
}

// ── Framework-specific scanning ──────────────────────────────────────────────

QVector<CodeGraphConnectionInfo> PythonLanguageIndexer::scanConnections(const QString & /*content*/) const
{
    // PyQt signal/slot connections could be detected here in the future.
    return {};
}

QVector<CodeGraphServiceRef> PythonLanguageIndexer::scanServiceRefs(const QString & /*content*/) const
{
    return {};
}

QVector<CodeGraphTestCase> PythonLanguageIndexer::scanTestCases(const QString &content) const
{
    QVector<CodeGraphTestCase> results;

    // pytest convention: def test_xxx()
    auto testIt = RE_PY_TEST.globalMatch(content);
    while (testIt.hasNext()) {
        auto m = testIt.next();
        CodeGraphTestCase tc;
        tc.methodName = m.captured(1);
        tc.lineNum    = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        results.append(tc);
    }

    return results;
}
