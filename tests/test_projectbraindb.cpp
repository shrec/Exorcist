#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QTest>

#include "agent/tools/projectbraindb.h"

class TestProjectBrainDb : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testNoWorkspaceReturnsError();
    void testInitCreatesSchemaAndScans();
    void testStatusReportsCorrectCounts();
    void testScanIncrementalSkipsUnchanged();
    void testScanIncrementalDetectsChanges();
    void testSymbolExtractionCpp();
    void testSymbolExtractionPython();
    void testIncludeDependencies();
    void testLearnAndQuery();
    void testForget();
    void testQueryBlocksWrites();
    void testExecuteRunsDDL();
    void testUnknownOperation();
    void testDetectLanguageExtensions();

private:
    QTemporaryDir m_tmpDir;

    void writeFile(const QString &relPath, const QString &content);
    ToolExecResult invoke(ProjectBrainDbTool &tool, const QJsonObject &args);
};

void TestProjectBrainDb::initTestCase()
{
    QVERIFY(m_tmpDir.isValid());
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
        QSKIP("QSQLITE driver not available (set QT_PLUGIN_PATH)");
}

void TestProjectBrainDb::cleanupTestCase()
{
    // QTemporaryDir auto-cleans
}

void TestProjectBrainDb::writeFile(const QString &relPath, const QString &content)
{
    const QString abs = m_tmpDir.path() + QLatin1Char('/') + relPath;
    QDir().mkpath(QFileInfo(abs).path());
    QFile f(abs);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(content.toUtf8());
}

ToolExecResult TestProjectBrainDb::invoke(ProjectBrainDbTool &tool,
                                          const QJsonObject &args)
{
    return tool.invoke(args);
}

// ── tests ─────────────────────────────────────────────────────────────────────

void TestProjectBrainDb::testNoWorkspaceReturnsError()
{
    ProjectBrainDbTool tool;
    // No workspace root set
    auto r = invoke(tool, {{QStringLiteral("operation"), QStringLiteral("init")}});
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("workspace")));
}

void TestProjectBrainDb::testInitCreatesSchemaAndScans()
{
    // Create sample files
    writeFile(QStringLiteral("main.cpp"),
              QStringLiteral("#include <iostream>\nint main() { return 0; }\n"));
    writeFile(QStringLiteral("hello.py"),
              QStringLiteral("class Greeter:\n    def greet(self):\n        pass\n"));

    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {{QStringLiteral("operation"), QStringLiteral("init")}});
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("files scanned")));

    // DB file should exist
    QVERIFY(QFileInfo::exists(m_tmpDir.path() + QStringLiteral("/.exorcist/project.db")));

    // Structured data should have filesScanned
    QVERIFY(r.data.contains(QLatin1String("filesScanned")));
    QVERIFY(r.data[QLatin1String("filesScanned")].toInt() >= 2);
}

void TestProjectBrainDb::testStatusReportsCorrectCounts()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {{QStringLiteral("operation"), QStringLiteral("status")}});
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.data[QLatin1String("files")].toInt() >= 2);
    QVERIFY(r.data[QLatin1String("symbols")].toInt() > 0);
    QVERIFY(r.textContent.contains(QStringLiteral("Files:")));
    QVERIFY(r.textContent.contains(QStringLiteral("Symbols:")));
}

void TestProjectBrainDb::testScanIncrementalSkipsUnchanged()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {{QStringLiteral("operation"), QStringLiteral("scan")}});
    QVERIFY2(r.ok, qUtf8Printable(r.error));

    // Nothing changed, so 0 files updated
    QCOMPARE(r.data[QLatin1String("filesUpdated")].toInt(), 0);
}

void TestProjectBrainDb::testScanIncrementalDetectsChanges()
{
    // Modify a file
    writeFile(QStringLiteral("main.cpp"),
              QStringLiteral("#include <iostream>\nint main() { return 42; }\n"
                             "void newFunc() {}\n"));

    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {{QStringLiteral("operation"), QStringLiteral("scan")}});
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.data[QLatin1String("filesUpdated")].toInt() >= 1);
}

void TestProjectBrainDb::testSymbolExtractionCpp()
{
    writeFile(QStringLiteral("src/widget.h"),
              QStringLiteral("class MyWidget {\n"
                             "public:\n"
                             "    void render();\n"
                             "};\n"
                             "enum class Color { Red, Blue };\n"));

    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    // Re-init to pick up new file
    invoke(tool, {{QStringLiteral("operation"), QStringLiteral("init")}});

    // Query for the class symbol
    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT name, kind FROM symbols WHERE name = 'MyWidget'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("MyWidget")));
    QVERIFY(r.textContent.contains(QStringLiteral("class")));

    // Query for the enum
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT name, kind FROM symbols WHERE name = 'Color'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("Color")));
    QVERIFY(r.textContent.contains(QStringLiteral("enum")));
}

void TestProjectBrainDb::testSymbolExtractionPython()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    // Query for Python symbols from hello.py (created in testInit)
    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT name, kind FROM symbols WHERE file_path = 'hello.py'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("Greeter")));
    QVERIFY(r.textContent.contains(QStringLiteral("greet")));
}

void TestProjectBrainDb::testIncludeDependencies()
{
    writeFile(QStringLiteral("src/foo.cpp"),
              QStringLiteral("#include \"foo.h\"\n#include <vector>\n"
                             "void doFoo() {}\n"));

    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());
    invoke(tool, {{QStringLiteral("operation"), QStringLiteral("init")}});

    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT target_file, kind FROM dependencies "
                        "WHERE source_file = 'src/foo.cpp'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("foo.h")));
    QVERIFY(r.textContent.contains(QStringLiteral("vector")));
    QVERIFY(r.textContent.contains(QStringLiteral("include")));
}

void TestProjectBrainDb::testLearnAndQuery()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    // Learn a fact
    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("learn")},
        {QStringLiteral("key"), QStringLiteral("build_system")},
        {QStringLiteral("value"), QStringLiteral("CMake with Ninja generator")},
        {QStringLiteral("category"), QStringLiteral("build")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("Learned")));

    // Query it back
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT key, value, category FROM knowledge "
                        "WHERE key = 'build_system'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("build_system")));
    QVERIFY(r.textContent.contains(QStringLiteral("CMake with Ninja")));
    QVERIFY(r.textContent.contains(QStringLiteral("build")));

    // Learn with empty key should fail
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("learn")},
        {QStringLiteral("key"), QStringLiteral("")},
        {QStringLiteral("value"), QStringLiteral("oops")}
    });
    QVERIFY(!r.ok);
}

void TestProjectBrainDb::testForget()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    // Learn then forget
    invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("learn")},
        {QStringLiteral("key"), QStringLiteral("temp_fact")},
        {QStringLiteral("value"), QStringLiteral("temporary")},
        {QStringLiteral("category"), QStringLiteral("test")}
    });

    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("forget")},
        {QStringLiteral("key"), QStringLiteral("temp_fact")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("Forgot")));

    // Verify it's gone
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT * FROM knowledge WHERE key = 'temp_fact'")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("0 row(s)")));

    // Forget non-existent key
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("forget")},
        {QStringLiteral("key"), QStringLiteral("nonexistent")}
    });
    QVERIFY(r.ok); // succeeds but says "not found"
    QVERIFY(r.textContent.contains(QStringLiteral("not found")));

    // Forget with empty key
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("forget")},
        {QStringLiteral("key"), QStringLiteral("")}
    });
    QVERIFY(!r.ok);
}

void TestProjectBrainDb::testQueryBlocksWrites()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    // Attempt INSERT through query — should be rejected
    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("INSERT INTO knowledge (key, value) VALUES ('hack', 'bad')")}
    });
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("Write")));

    // Attempt DELETE through query
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"), QStringLiteral("DELETE FROM knowledge")}
    });
    QVERIFY(!r.ok);

    // Attempt DROP through query
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"), QStringLiteral("DROP TABLE knowledge")}
    });
    QVERIFY(!r.ok);
}

void TestProjectBrainDb::testExecuteRunsDDL()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("execute")},
        {QStringLiteral("sql"),
         QStringLiteral("CREATE TABLE IF NOT EXISTS custom_notes "
                        "(id INTEGER PRIMARY KEY, note TEXT)")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("Executed")));

    // Insert a row
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("execute")},
        {QStringLiteral("sql"),
         QStringLiteral("INSERT INTO custom_notes (note) VALUES ('hello')")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));

    // Query it
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT note FROM custom_notes")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("hello")));

    // Execute with empty SQL
    r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("execute")},
        {QStringLiteral("sql"), QStringLiteral("")}
    });
    QVERIFY(!r.ok);
}

void TestProjectBrainDb::testUnknownOperation()
{
    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());

    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("foobar")}
    });
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("Unknown")));
}

void TestProjectBrainDb::testDetectLanguageExtensions()
{
    // Test that various extensions produce files in DB with correct language
    writeFile(QStringLiteral("test.rs"),
              QStringLiteral("pub fn hello() {}\npub struct Point { x: f64, y: f64 }\n"));
    writeFile(QStringLiteral("test.js"),
              QStringLiteral("function greet() {}\nclass App {}\n"));

    ProjectBrainDbTool tool;
    tool.setWorkspaceRoot(m_tmpDir.path());
    invoke(tool, {{QStringLiteral("operation"), QStringLiteral("init")}});

    auto r = invoke(tool, {
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sql"),
         QStringLiteral("SELECT DISTINCT language FROM files ORDER BY language")}
    });
    QVERIFY2(r.ok, qUtf8Printable(r.error));
    QVERIFY(r.textContent.contains(QStringLiteral("cpp")));
    QVERIFY(r.textContent.contains(QStringLiteral("python")));
    QVERIFY(r.textContent.contains(QStringLiteral("rust")));
    QVERIFY(r.textContent.contains(QStringLiteral("javascript")));
}

QTEST_MAIN(TestProjectBrainDb)
#include "test_projectbraindb.moc"
