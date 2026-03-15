#include "cpplanguageindexer.h"
#include "codegraphutils.h"

#include <QRegularExpression>
#include <QSet>

// ══════════════════════════════════════════════════════════════════════════════
// C++ regex patterns
// ══════════════════════════════════════════════════════════════════════════════

static const QRegularExpression RE_CLASS(
    QStringLiteral(R"(^\s*(?:class|struct)\s+(?:Q_DECL_EXPORT\s+|Q_DECL_IMPORT\s+)?(\w+))"
                   R"((?:\s*:\s*(?:public|protected|private)\s+([\w:,\s]+?))?\s*\{)"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_INCLUDE(
    QStringLiteral(R"(^\s*#include\s+[<"]([^>"]+)[>"])"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_PURE_VIRTUAL(
    QStringLiteral(R"(=\s*0\s*;)"));

static const QRegularExpression RE_NAMESPACE(
    QStringLiteral(R"(^\s*namespace\s+(\w+))"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_METHOD_DEF(
    QStringLiteral(R"((\w+)::(~?\w+)\s*\()"));

static const QRegularExpression RE_FREE_FUNC(
    QStringLiteral(R"(^(?:static\s+|inline\s+|constexpr\s+|extern\s+)*)"
                   R"((?:[\w:*&<>, ]+\s+)(\w+)\s*\()"));

static const QRegularExpression RE_CONNECT(
    QStringLiteral(R"(connect\s*\(\s*(\w+)\s*,\s*&(\w+)::\s*(\w+)\s*,)"
                   R"(\s*(\w+)\s*,\s*&(\w+)::\s*(\w+))"));

static const QRegularExpression RE_SERVICE_REG(
    QStringLiteral(R"RE(registerService\s*\(\s*"([^"]+)")RE"));
static const QRegularExpression RE_SERVICE_RES(
    QStringLiteral(R"RE(resolveService\s*\(\s*"([^"]+)")RE"));

static const QRegularExpression RE_TEST_SLOT(
    QStringLiteral(R"(^\s*void\s+(test\w+)\s*\(\s*\))"),
    QRegularExpression::MultilineOption);

static const QRegularExpression RE_QTEST_MAIN(
    QStringLiteral(R"(QTEST_MAIN\s*\(\s*(\w+)\s*\))"));

static const QSet<QString> SKIP_KEYWORDS = {
    QStringLiteral("if"),       QStringLiteral("else"),
    QStringLiteral("for"),      QStringLiteral("while"),
    QStringLiteral("switch"),   QStringLiteral("do"),
    QStringLiteral("return"),   QStringLiteral("catch"),
    QStringLiteral("try"),      QStringLiteral("case"),
    QStringLiteral("class"),    QStringLiteral("struct"),
    QStringLiteral("enum"),     QStringLiteral("namespace"),
    QStringLiteral("typedef"),  QStringLiteral("using"),
    QStringLiteral("template"), QStringLiteral("throw"),
    QStringLiteral("delete"),
};

// ══════════════════════════════════════════════════════════════════════════════

QString CppLanguageIndexer::languageId() const
{
    return QStringLiteral("cpp");
}

QStringList CppLanguageIndexer::fileExtensions() const
{
    return {
        QStringLiteral(".h"),   QStringLiteral(".hpp"),
        QStringLiteral(".cpp"), QStringLiteral(".cc"),
        QStringLiteral(".cxx"),
    };
}

// ── File metadata ────────────────────────────────────────────────────────────

CodeGraphFileMetadata CppLanguageIndexer::extractFileMetadata(const QString &content) const
{
    CodeGraphFileMetadata meta;
    meta.hasQObject = content.contains(QLatin1String("Q_OBJECT"));
    meta.hasSignals = content.contains(QLatin1String("signals:"));
    meta.hasSlots   = content.contains(QLatin1String("slots:"));
    return meta;
}

// ── Structural parsing ──────────────────────────────────────────────────────

CodeGraphParseResult CppLanguageIndexer::parseFile(const QString &content) const
{
    CodeGraphParseResult result;

    // ── Includes ──
    auto includeIt = RE_INCLUDE.globalMatch(content);
    while (includeIt.hasNext()) {
        auto m = includeIt.next();
        CodeGraphImportInfo imp;
        imp.path = m.captured(1);
        imp.isSystem = content.mid(m.capturedStart(), 20).trimmed()
                           .startsWith(QLatin1String("#include <"));
        result.imports.append(imp);
    }

    // ── Namespaces ──
    auto nsIt = RE_NAMESPACE.globalMatch(content);
    while (nsIt.hasNext()) {
        auto m = nsIt.next();
        CodeGraphNamespaceInfo ns;
        ns.name = m.captured(1);
        result.namespaces.append(ns);
    }

    // ── Classes / structs ──
    auto classIt = RE_CLASS.globalMatch(content);
    while (classIt.hasNext()) {
        auto m = classIt.next();
        const QString clsName = m.captured(1);
        const QString bases   = m.captured(2).trimmed();
        const int lineNum = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;

        // Find class body to check for pure virtual
        int braceStart = m.capturedEnd() - 1;
        int depth = 1;
        int pos = braceStart + 1;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == QLatin1Char('{'))
                ++depth;
            else if (content[pos] == QLatin1Char('}'))
                --depth;
            ++pos;
        }
        const QString classBody = content.mid(braceStart, pos - braceStart);
        const bool isIface = RE_PURE_VIRTUAL.match(classBody).hasMatch();
        const bool isStruct = content.mid(m.capturedStart(), 20).trimmed()
                                  .startsWith(QLatin1String("struct"));

        CodeGraphClassInfo cls;
        cls.name        = clsName;
        cls.bases       = bases;
        cls.lineNum     = lineNum;
        cls.isInterface = isIface;
        cls.isStruct    = isStruct;
        result.classes.append(cls);
    }

    return result;
}

// ── Function extraction ──────────────────────────────────────────────────────

QVector<CodeGraphFunction> CppLanguageIndexer::extractFunctions(const QString &content) const
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    QVector<CodeGraphFunction> functions;
    const int n = lines.size();
    int i = 0;
    bool inBlockComment = false;

    while (i < n) {
        const QString &line = lines[i];
        const QString stripped = line.trimmed();

        // Skip block comments
        if (inBlockComment) {
            if (stripped.contains(QLatin1String("*/")))
                inBlockComment = false;
            ++i;
            continue;
        }
        if (stripped.startsWith(QLatin1String("/*")) &&
            !stripped.contains(QLatin1String("*/"))) {
            inBlockComment = true;
            ++i;
            continue;
        }

        // Skip empty, preprocessor, line comments
        if (stripped.isEmpty() ||
            stripped.startsWith(QLatin1Char('#')) ||
            stripped.startsWith(QLatin1String("//"))) {
            ++i;
            continue;
        }

        // Method definition: ClassName::methodName(
        auto m = RE_METHOD_DEF.match(stripped);
        if (m.hasMatch()) {
            const QString className  = m.captured(1);
            const QString methodName = m.captured(2);
            if (!SKIP_KEYWORDS.contains(methodName)) {
                const int endIdx = CodeGraphUtils::findBraceBody(lines, i, n);
                if (endIdx >= 0) {
                    CodeGraphFunction fn;
                    fn.name          = methodName;
                    fn.qualifiedName = className + QStringLiteral("::") + methodName;
                    fn.className     = className;
                    fn.isMethod      = true;
                    fn.startLine     = i + 1;
                    fn.endLine       = endIdx + 1;
                    fn.lineCount     = endIdx - i + 1;
                    functions.append(fn);
                    i = endIdx + 1;
                    continue;
                }
            }
        }

        // Free function at column 0
        if (!line.isEmpty() && !line[0].isSpace() &&
            stripped.contains(QLatin1Char('(')) &&
            !stripped.contains(QLatin1String("::"))) {
            auto fm = RE_FREE_FUNC.match(stripped);
            if (fm.hasMatch() && !SKIP_KEYWORDS.contains(fm.captured(1))) {
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
                    i = endIdx + 1;
                    continue;
                }
            }
        }

        ++i;
    }

    return functions;
}

// ── First comment extraction ─────────────────────────────────────────────────

QString CppLanguageIndexer::extractFirstComment(const QStringList &lines) const
{
    for (int li = 0; li < qMin(30, lines.size()); ++li) {
        const QString s = lines[li].trimmed();
        if (s.startsWith(QLatin1String("///")) ||
            s.startsWith(QLatin1String("//!"))) {
            QString comment = s;
            while (comment.startsWith(QLatin1Char('/')) ||
                   comment.startsWith(QLatin1Char('!')))
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
        if (s.startsWith(QLatin1String("#pragma once")) ||
            s.startsWith(QLatin1String("#include")))
            continue;
        if (!s.isEmpty() && !s.startsWith(QLatin1String("/*")) &&
            !s.startsWith(QLatin1String("//")))
            break;
    }
    return {};
}

// ── Qt connections ───────────────────────────────────────────────────────────

QVector<CodeGraphConnectionInfo> CppLanguageIndexer::scanConnections(const QString &content) const
{
    QVector<CodeGraphConnectionInfo> results;

    auto it = RE_CONNECT.globalMatch(content);
    while (it.hasNext()) {
        auto m = it.next();
        CodeGraphConnectionInfo conn;
        conn.lineNum    = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        conn.sender     = m.captured(1) + QStringLiteral(" (") + m.captured(2) + QLatin1Char(')');
        conn.signalName = m.captured(3);
        conn.receiver   = m.captured(4) + QStringLiteral(" (") + m.captured(5) + QLatin1Char(')');
        conn.slotName   = m.captured(6);
        results.append(conn);
    }

    return results;
}

// ── ServiceRegistry scanning ─────────────────────────────────────────────────

QVector<CodeGraphServiceRef> CppLanguageIndexer::scanServiceRefs(const QString &content) const
{
    QVector<CodeGraphServiceRef> results;

    auto regIt = RE_SERVICE_REG.globalMatch(content);
    while (regIt.hasNext()) {
        auto m = regIt.next();
        CodeGraphServiceRef ref;
        ref.key     = m.captured(1);
        ref.lineNum = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        ref.regType = QStringLiteral("register");
        results.append(ref);
    }

    auto resIt = RE_SERVICE_RES.globalMatch(content);
    while (resIt.hasNext()) {
        auto m = resIt.next();
        CodeGraphServiceRef ref;
        ref.key     = m.captured(1);
        ref.lineNum = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        ref.regType = QStringLiteral("resolve");
        results.append(ref);
    }

    return results;
}

// ── QTest scanning ───────────────────────────────────────────────────────────

QVector<CodeGraphTestCase> CppLanguageIndexer::scanTestCases(const QString &content) const
{
    QVector<CodeGraphTestCase> results;

    // Find test class name from QTEST_MAIN
    QString testClass;
    auto cm = RE_QTEST_MAIN.match(content);
    if (cm.hasMatch())
        testClass = cm.captured(1);

    // Find test slots
    auto slotIt = RE_TEST_SLOT.globalMatch(content);
    while (slotIt.hasNext()) {
        auto m = slotIt.next();
        CodeGraphTestCase tc;
        tc.className  = testClass;
        tc.methodName = m.captured(1);
        tc.lineNum    = content.left(m.capturedStart()).count(QLatin1Char('\n')) + 1;
        results.append(tc);
    }

    return results;
}
