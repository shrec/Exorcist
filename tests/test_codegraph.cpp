#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "codegraph/codegraphdb.h"
#include "codegraph/codegraphindexer.h"
#include "codegraph/codegraphquery.h"
#include "codegraph/cpplanguageindexer.h"
#include "codegraph/pylanguageindexer.h"

class TestCodeGraph : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // ── extractCppFunctions ──
    void testExtractSimpleFunction();
    void testExtractMethod();
    void testExtractMultipleFunctions();
    void testExtractSkipsComments();
    void testExtractSkipsKeywords();
    void testExtractNestedBraces();

    // ── extractPyFunctions ──
    void testExtractPyFreeFunction();
    void testExtractPyMethod();
    void testExtractPyMultiple();

    // ── Database ──
    void testDbOpenClose();
    void testSchemaCreation();
    void testDefaultPath();

    // ── Full pipeline ──
    void testScanFiles();
    void testParseAll();
    void testBuildFunctionIndex();
    void testFullRebuild();

    // ── Query ──
    void testProjectSummary();
    void testFunctionQuery();
    void testContextQuery();
};

// ══════════════════════════════════════════════════════════════════════════════
// Init
// ══════════════════════════════════════════════════════════════════════════════

void TestCodeGraph::initTestCase()
{
    // Algorithm tests don't need QSQLITE; DB tests will be skipped individually.
}

// ══════════════════════════════════════════════════════════════════════════════
// extractCppFunctions tests
// ══════════════════════════════════════════════════════════════════════════════

void TestCodeGraph::testExtractSimpleFunction()
{
    const QString code =
        "int main(int argc, char **argv)\n"
        "{\n"
        "    return 0;\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("main"));
    QCOMPARE(funcs[0].isMethod, false);
    QCOMPARE(funcs[0].startLine, 1);
    QCOMPARE(funcs[0].endLine, 4);
    QCOMPARE(funcs[0].lineCount, 4);
}

void TestCodeGraph::testExtractMethod()
{
    const QString code =
        "void MyClass::doWork()\n"
        "{\n"
        "    m_running = true;\n"
        "    process();\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("doWork"));
    QCOMPARE(funcs[0].qualifiedName, QStringLiteral("MyClass::doWork"));
    QCOMPARE(funcs[0].className, QStringLiteral("MyClass"));
    QCOMPARE(funcs[0].isMethod, true);
    QCOMPARE(funcs[0].startLine, 1);
    QCOMPARE(funcs[0].endLine, 5);
}

void TestCodeGraph::testExtractMultipleFunctions()
{
    const QString code =
        "int foo()\n"
        "{\n"
        "    return 1;\n"
        "}\n"
        "\n"
        "void Widget::bar(int x)\n"
        "{\n"
        "    m_x = x;\n"
        "}\n"
        "\n"
        "static void helper()\n"
        "{\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 3);
    QCOMPARE(funcs[0].name, QStringLiteral("foo"));
    QCOMPARE(funcs[0].isMethod, false);
    QCOMPARE(funcs[1].name, QStringLiteral("bar"));
    QCOMPARE(funcs[1].className, QStringLiteral("Widget"));
    QCOMPARE(funcs[1].isMethod, true);
    QCOMPARE(funcs[2].name, QStringLiteral("helper"));
}

void TestCodeGraph::testExtractSkipsComments()
{
    const QString code =
        "// void notAFunction()\n"
        "// {\n"
        "// }\n"
        "/* block comment\n"
        "void alsoNotAFunction()\n"
        "{\n"
        "}\n"
        "*/\n"
        "void realFunction()\n"
        "{\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("realFunction"));
}

void TestCodeGraph::testExtractSkipsKeywords()
{
    const QString code =
        "void Foo::doStuff()\n"
        "{\n"
        "    if (x) {\n"
        "        doSomething();\n"
        "    }\n"
        "    for (int i = 0; i < n; ++i) {\n"
        "        process(i);\n"
        "    }\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("doStuff"));
    QCOMPARE(funcs[0].lineCount, 9);
}

void TestCodeGraph::testExtractNestedBraces()
{
    const QString code =
        "void deep()\n"
        "{\n"
        "    {\n"
        "        {\n"
        "            int x = 0;\n"
        "        }\n"
        "    }\n"
        "}\n";

    auto funcs = CppLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("deep"));
    QCOMPARE(funcs[0].startLine, 1);
    QCOMPARE(funcs[0].endLine, 8);
}

// ══════════════════════════════════════════════════════════════════════════════
// extractPyFunctions tests
// ══════════════════════════════════════════════════════════════════════════════

void TestCodeGraph::testExtractPyFreeFunction()
{
    const QString code =
        "def hello(name):\n"
        "    print(f'Hello {name}')\n"
        "    return True\n"
        "\n"
        "x = 1\n";

    auto funcs = PythonLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 1);
    QCOMPARE(funcs[0].name, QStringLiteral("hello"));
    QCOMPARE(funcs[0].isMethod, false);
    QCOMPARE(funcs[0].startLine, 1);
}

void TestCodeGraph::testExtractPyMethod()
{
    const QString code =
        "class MyClass:\n"
        "    def __init__(self):\n"
        "        self.x = 0\n"
        "\n"
        "    def process(self):\n"
        "        return self.x + 1\n";

    auto funcs = PythonLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 2);
    QCOMPARE(funcs[0].name, QStringLiteral("__init__"));
    QCOMPARE(funcs[0].className, QStringLiteral("MyClass"));
    QCOMPARE(funcs[0].isMethod, true);
    QCOMPARE(funcs[1].name, QStringLiteral("process"));
    QCOMPARE(funcs[1].className, QStringLiteral("MyClass"));
}

void TestCodeGraph::testExtractPyMultiple()
{
    const QString code =
        "def a():\n"
        "    pass\n"
        "\n"
        "def b():\n"
        "    pass\n"
        "\n"
        "def c():\n"
        "    pass\n";

    auto funcs = PythonLanguageIndexer().extractFunctions(code);
    QCOMPARE(funcs.size(), 3);
    QCOMPARE(funcs[0].name, QStringLiteral("a"));
    QCOMPARE(funcs[1].name, QStringLiteral("b"));
    QCOMPARE(funcs[2].name, QStringLiteral("c"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Database tests
// ══════════════════════════════════════════════════════════════════════════════

void TestCodeGraph::testDbOpenClose()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    CodeGraphDb db;
    QVERIFY(db.open(tmpDir.filePath(QStringLiteral("test.db"))));
    QVERIFY(db.isOpen());
    db.close();
    QVERIFY(!db.isOpen());
}

void TestCodeGraph::testSchemaCreation()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    CodeGraphDb db;
    QVERIFY(db.open(tmpDir.filePath(QStringLiteral("test.db"))));
    QVERIFY(db.createSchema());

    // Verify key tables exist
    QSqlQuery q(db.database());
    q.exec(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table'"));
    QVERIFY(q.next());
    const int tableCount = q.value(0).toInt();
    QVERIFY2(tableCount >= 17,
             qPrintable(QStringLiteral("Expected >=17 tables, got %1").arg(tableCount)));

    // Verify specific tables
    for (const auto *tbl : {"files", "classes", "methods", "includes",
                            "function_index", "edges", "qt_connections",
                            "services", "test_cases", "cmake_targets"}) {
        q.prepare(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?"));
        q.bindValue(0, QString::fromLatin1(tbl));
        q.exec();
        QVERIFY2(q.next(), qPrintable(QStringLiteral("Missing table: %1")
                                          .arg(QString::fromLatin1(tbl))));
    }

    db.close();
}

void TestCodeGraph::testDefaultPath()
{
    const auto path = CodeGraphDb::defaultPath(QStringLiteral("/workspace"));
    QCOMPARE(path, QStringLiteral("/workspace/.exorcist/codegraph.db"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Pipeline tests (using temp workspace)
// ══════════════════════════════════════════════════════════════════════════════

static void createTestWorkspace(const QString &root)
{
    // src/editor/texteditor.h
    QDir(root).mkpath(QStringLiteral("src/editor"));
    {
        QFile f(root + QStringLiteral("/src/editor/texteditor.h"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "#pragma once\n"
            "#include <QWidget>\n"
            "/// Simple text editor widget\n"
            "class TextEditor : public QWidget\n"
            "{\n"
            "    Q_OBJECT\n"
            "signals:\n"
            "    void textChanged();\n"
            "public slots:\n"
            "    void clear();\n"
            "public:\n"
            "    virtual void undo() = 0;\n"
            "};\n");
    }

    // src/editor/texteditor.cpp
    {
        QFile f(root + QStringLiteral("/src/editor/texteditor.cpp"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "#include \"texteditor.h\"\n"
            "\n"
            "void TextEditor::clear()\n"
            "{\n"
            "    // clear text\n"
            "}\n"
            "\n"
            "static int helper(int x)\n"
            "{\n"
            "    return x + 1;\n"
            "}\n");
    }

    // src/mainwindow.cpp (with connect and service)
    {
        QFile f(root + QStringLiteral("/src/mainwindow.cpp"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "#include \"mainwindow.h\"\n"
            "#include \"editor/texteditor.h\"\n"
            "\n"
            "void MainWindow::setup()\n"
            "{\n"
            "    connect(m_editor, &TextEditor::textChanged, this, &MainWindow::onEdit);\n"
            "    registerService(\"editor\");\n"
            "    auto svc = resolveService(\"theme\");\n"
            "}\n");
    }

    // src/mainwindow.h
    {
        QFile f(root + QStringLiteral("/src/mainwindow.h"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "#pragma once\n"
            "class MainWindow : public QMainWindow\n"
            "{\n"
            "    Q_OBJECT\n"
            "public:\n"
            "    void setup();\n"
            "public slots:\n"
            "    void onEdit();\n"
            "};\n");
    }

    // tests/test_editor.cpp
    QDir(root).mkpath(QStringLiteral("tests"));
    {
        QFile f(root + QStringLiteral("/tests/test_editor.cpp"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "#include <QTest>\n"
            "class TestEditor : public QObject\n"
            "{\n"
            "    Q_OBJECT\n"
            "private slots:\n"
            "    void testClear();\n"
            "    void testUndo();\n"
            "};\n"
            "void TestEditor::testClear() {}\n"
            "void TestEditor::testUndo() {}\n"
            "QTEST_MAIN(TestEditor)\n");
    }

    // CMakeLists.txt
    {
        QFile f(root + QStringLiteral("/CMakeLists.txt"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "add_executable(myapp src/mainwindow.cpp)\n"
            "add_library(editor MODULE src/editor/texteditor.cpp)\n"
            "add_test(NAME EditorTest COMMAND test_editor)\n");
    }

    // tools/helper.py
    QDir(root).mkpath(QStringLiteral("tools"));
    {
        QFile f(root + QStringLiteral("/tools/helper.py"));
        f.open(QIODevice::WriteOnly);
        f.write(
            "# Helper utility\n"
            "def scan_files(path):\n"
            "    import os\n"
            "    return os.listdir(path)\n"
            "\n"
            "class Scanner:\n"
            "    def run(self):\n"
            "        return self.scan_files('.')\n");
    }
}

void TestCodeGraph::testScanFiles()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    const int count = indexer.scanFiles(cgdb.database());

    // Should find: texteditor.h, texteditor.cpp, mainwindow.h, mainwindow.cpp,
    //              test_editor.cpp, helper.py (6 code files)
    QVERIFY2(count >= 6,
             qPrintable(QStringLiteral("Expected >=6 files, got %1").arg(count)));

    cgdb.close();
}

void TestCodeGraph::testParseAll()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    indexer.scanFiles(cgdb.database());
    indexer.parseAll(cgdb.database());

    // Check classes were found
    QSqlQuery q(cgdb.database());
    q.exec(QStringLiteral("SELECT COUNT(*) FROM classes"));
    QVERIFY(q.next());
    QVERIFY2(q.value(0).toInt() >= 2,
             qPrintable(QStringLiteral("Expected >=2 classes, got %1")
                            .arg(q.value(0).toInt())));

    // Check includes were found
    q.exec(QStringLiteral("SELECT COUNT(*) FROM includes"));
    QVERIFY(q.next());
    QVERIFY2(q.value(0).toInt() >= 2,
             qPrintable(QStringLiteral("Expected >=2 includes, got %1")
                            .arg(q.value(0).toInt())));

    cgdb.close();
}

void TestCodeGraph::testBuildFunctionIndex()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    indexer.scanFiles(cgdb.database());
    indexer.parseAll(cgdb.database());
    const int funcs = indexer.buildFunctionIndex(cgdb.database());

    // texteditor.cpp has TextEditor::clear + helper = 2
    // mainwindow.cpp has MainWindow::setup = 1
    // test_editor.cpp has TestEditor::testClear + TestEditor::testUndo = 2
    // helper.py has scan_files + Scanner.run = 2
    QVERIFY2(funcs >= 5,
             qPrintable(QStringLiteral("Expected >=5 functions, got %1").arg(funcs)));

    // Verify line ranges are sensible
    QSqlQuery q(cgdb.database());
    q.exec(QStringLiteral(
        "SELECT name, start_line, end_line, line_count FROM function_index "
        "WHERE start_line > 0 AND end_line >= start_line"));
    int validCount = 0;
    while (q.next()) {
        QVERIFY(q.value(1).toInt() > 0);
        QVERIFY(q.value(2).toInt() >= q.value(1).toInt());
        QVERIFY(q.value(3).toInt() > 0);
        ++validCount;
    }
    QVERIFY(validCount >= 5);

    cgdb.close();
}

void TestCodeGraph::testFullRebuild()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    auto stats = indexer.fullRebuild(cgdb.database(), cgdb.hasFts5());

    QVERIFY(stats.filesIndexed >= 6);
    QVERIFY(stats.classesParsed >= 1);
    QVERIFY(stats.functionsExtracted >= 5);
    QVERIFY(stats.edgesBuilt >= 1);
    QVERIFY(stats.qtConnections >= 1);
    QVERIFY(stats.servicesFound >= 2);  // 1 register + 1 resolve
    QVERIFY(stats.testCases >= 2);      // testClear, testUndo
    QVERIFY(stats.cmakeTargets >= 3);   // myapp, editor, EditorTest

    cgdb.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// Query tests
// ══════════════════════════════════════════════════════════════════════════════

void TestCodeGraph::testProjectSummary()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    indexer.fullRebuild(cgdb.database(), cgdb.hasFts5());

    CodeGraphQuery query(cgdb.database());
    auto summary = query.projectSummary();

    QVERIFY(summary[QStringLiteral("files")].toInt() >= 6);
    QVERIFY(summary[QStringLiteral("classes")].toInt() >= 2);
    QVERIFY(summary[QStringLiteral("functions")].toInt() >= 5);
    QVERIFY(summary[QStringLiteral("edges")].toInt() >= 1);
    QVERIFY(summary[QStringLiteral("connections")].toInt() >= 1);
    QVERIFY(summary[QStringLiteral("services")].toInt() >= 2);
    QVERIFY(summary[QStringLiteral("test_cases")].toInt() >= 2);
    QVERIFY(summary[QStringLiteral("cmake_targets")].toInt() >= 3);

    cgdb.close();
}

void TestCodeGraph::testFunctionQuery()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    indexer.fullRebuild(cgdb.database(), cgdb.hasFts5());

    CodeGraphQuery query(cgdb.database());

    // Query by file path
    auto funcs = query.functions(QStringLiteral("texteditor.cpp"));
    QVERIFY2(funcs.size() >= 2,
             qPrintable(QStringLiteral("Expected >=2 functions, got %1")
                            .arg(funcs.size())));

    // Verify first function has required fields
    auto first = funcs[0].toObject();
    QVERIFY(first.contains(QStringLiteral("name")));
    QVERIFY(first.contains(QStringLiteral("start_line")));
    QVERIFY(first.contains(QStringLiteral("end_line")));
    QVERIFY(first[QStringLiteral("start_line")].toInt() > 0);

    cgdb.close();
}

void TestCodeGraph::testContextQuery()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestWorkspace(tmpDir.path());

    CodeGraphDb cgdb;
    QVERIFY(cgdb.open(tmpDir.filePath(QStringLiteral("codegraph.db"))));
    QVERIFY(cgdb.createSchema());

    CodeGraphIndexer indexer;
    indexer.setWorkspaceRoot(tmpDir.path());
    indexer.fullRebuild(cgdb.database(), cgdb.hasFts5());

    CodeGraphQuery query(cgdb.database());

    // Query by class name (bare name → class-first resolution)
    auto ctx = query.context(QStringLiteral("TextEditor"));
    QVERIFY(!ctx.isEmpty());
    QVERIFY(ctx[QStringLiteral("path")].toString().contains(
        QStringLiteral("texteditor")));
    QVERIFY(ctx.contains(QStringLiteral("lang")));
    QVERIFY(ctx.contains(QStringLiteral("summary")));

    // Query nonexistent target
    auto empty = query.context(QStringLiteral("NonExistentClass"));
    QVERIFY(empty.isEmpty());

    cgdb.close();
}

QTEST_MAIN(TestCodeGraph)
#include "test_codegraph.moc"
