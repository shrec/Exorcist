#include <QTest>
#include <QJsonObject>
#include <QJsonArray>

#include "agent/tools/treesitterquerytool.h"

// ── Test ──────────────────────────────────────────────────────────────────────

class TestTreeSitterQueryTool : public QObject
{
    Q_OBJECT

private:
    // Fake callbacks
    static QString fakeFileParser(const QString &filePath, int maxDepth)
    {
        Q_UNUSED(maxDepth)
        if (filePath.endsWith(QLatin1String(".cpp")))
            return QStringLiteral("(translation_unit (function_definition))");
        return {};
    }

    static TreeSitterQueryTool::QueryResult fakeQueryRunner(const QString &filePath,
                                                             const QString &pattern)
    {
        Q_UNUSED(filePath)
        if (pattern.contains(QLatin1String("function_definition"))) {
            TreeSitterQueryTool::QueryMatch m;
            m.captureName = QStringLiteral("fn");
            m.nodeType    = QStringLiteral("function_definition");
            m.startLine   = 5;
            m.startCol    = 0;
            m.endLine     = 15;
            m.endCol      = 1;
            m.text        = QStringLiteral("void foo() { ... }");
            return {true, {m}, 1, {}};
        }
        return {true, {}, 0, {}};
    }

    static QList<TreeSitterQueryTool::SymbolInfo> fakeSymbolExtractor(const QString &filePath)
    {
        if (filePath.isEmpty()) return {};
        return {
            {QStringLiteral("main"), QStringLiteral("function"), 0, 10, 0},
            {QStringLiteral("MyClass"), QStringLiteral("class"), 12, 50, 0},
            {QStringLiteral("helper"), QStringLiteral("function"), 52, 60, 0}
        };
    }

    static TreeSitterQueryTool::NodeInfo fakeNodeAtPosition(const QString &filePath,
                                                             int line, int column)
    {
        Q_UNUSED(filePath)
        if (line == 5 && column == 0)
            return {QStringLiteral("function_definition"), true, 5, 0, 15, 1,
                    QStringLiteral("void foo() { }"), 3};
        return {};
    }

    std::unique_ptr<TreeSitterQueryTool> makeTool()
    {
        return std::make_unique<TreeSitterQueryTool>(
            &fakeFileParser, &fakeQueryRunner,
            &fakeSymbolExtractor, &fakeNodeAtPosition);
    }

private slots:

    // ── spec ──────────────────────────────────────────────────────────────

    void spec_name()
    {
        auto tool = makeTool();
        QCOMPARE(tool->spec().name, QStringLiteral("tree_sitter_query"));
        QCOMPARE(tool->spec().permission, AgentToolPermission::ReadOnly);
    }

    // ── parse_file ────────────────────────────────────────────────────────

    void parseFile_validCpp()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("parse_file");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");

        auto result = tool->invoke(args);
        QVERIFY(result.ok);
        QVERIFY(result.textContent.contains(QLatin1String("translation_unit")));
    }

    void parseFile_unsupportedLanguage()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("parse_file");
        args[QLatin1String("filePath")]  = QStringLiteral("/readme.txt");

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
    }

    void parseFile_emptyPath()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("parse_file");
        args[QLatin1String("filePath")]  = QString();

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
    }

    // ── query ─────────────────────────────────────────────────────────────

    void query_withMatches()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")]    = QStringLiteral("query");
        args[QLatin1String("filePath")]     = QStringLiteral("/src/main.cpp");
        args[QLatin1String("queryPattern")] = QStringLiteral("(function_definition) @fn");

        auto result = tool->invoke(args);
        QVERIFY(result.ok);
        QCOMPARE(result.data[QLatin1String("matchCount")].toInt(), 1);
        QVERIFY(result.textContent.contains(QLatin1String("match")));
    }

    void query_noMatches()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")]    = QStringLiteral("query");
        args[QLatin1String("filePath")]     = QStringLiteral("/src/main.cpp");
        args[QLatin1String("queryPattern")] = QStringLiteral("(class_specifier) @cls");

        auto result = tool->invoke(args);
        QVERIFY(result.ok);
        QVERIFY(result.textContent.contains(QLatin1String("No matches")));
    }

    void query_missingPattern()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("query");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
        QVERIFY(result.error.contains(QLatin1String("queryPattern")));
    }

    // ── symbols ───────────────────────────────────────────────────────────

    void symbols_returnsList()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("symbols");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");

        auto result = tool->invoke(args);
        QVERIFY(result.ok);
        QCOMPARE(result.data[QLatin1String("symbolCount")].toInt(), 3);
        QVERIFY(result.textContent.contains(QLatin1String("main")));
        QVERIFY(result.textContent.contains(QLatin1String("MyClass")));
        QVERIFY(result.textContent.contains(QLatin1String("helper")));
    }

    void symbols_emptyFile()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("symbols");
        args[QLatin1String("filePath")]  = QString();

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
    }

    // ── node_at ───────────────────────────────────────────────────────────

    void nodeAt_found()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("node_at");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");
        args[QLatin1String("line")]      = 6;  // 1-based → converted to 0-based (5)
        args[QLatin1String("column")]    = 1;

        auto result = tool->invoke(args);
        QVERIFY(result.ok);
        QCOMPARE(result.data[QLatin1String("type")].toString(),
                 QStringLiteral("function_definition"));
        QCOMPARE(result.data[QLatin1String("childCount")].toInt(), 3);
    }

    void nodeAt_notFound()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("node_at");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");
        args[QLatin1String("line")]      = 999;
        args[QLatin1String("column")]    = 1;

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
    }

    void nodeAt_missingLine()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("node_at");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
        QVERIFY(result.error.contains(QLatin1String("line")));
    }

    // ── Unknown operation ─────────────────────────────────────────────────

    void unknownOperation()
    {
        auto tool = makeTool();
        QJsonObject args;
        args[QLatin1String("operation")] = QStringLiteral("invalid_op");
        args[QLatin1String("filePath")]  = QStringLiteral("/src/main.cpp");

        auto result = tool->invoke(args);
        QVERIFY(!result.ok);
        QVERIFY(result.error.contains(QLatin1String("Unknown operation")));
    }
};

QTEST_MAIN(TestTreeSitterQueryTool)
#include "test_treesitterquerytool.moc"
