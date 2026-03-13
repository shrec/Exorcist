#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "build/outputpanel.h"

class TestOutputPanel : public QObject
{
    Q_OBJECT

private:
    // Helper: feed lines into the panel and return problem list.
    static QList<OutputPanel::ProblemMatch> feedLines(
        OutputPanel &panel, const QStringList &lines)
    {
        for (const auto &line : lines)
            panel.appendBuildLine(line);
        return panel.problems();
    }

private slots:

    // ── GCC / Clang error with column ─────────────────────────────────────

    void gccErrorWithColumn()
    {
        OutputPanel panel;
        panel.appendBuildLine("main.cpp:10:5: error: undeclared identifier 'x'");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("main.cpp"));
        QCOMPARE(probs[0].line, 10);
        QCOMPARE(probs[0].column, 5);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Error);
        QVERIFY(probs[0].message.contains("undeclared"));
    }

    void gccFatalError()
    {
        OutputPanel panel;
        panel.appendBuildLine("header.h:1:1: fatal error: file not found");

        QCOMPARE(panel.problems().size(), 1);
        QCOMPARE(panel.problems()[0].severity, OutputPanel::ProblemMatch::Error);
    }

    void gccWarning()
    {
        OutputPanel panel;
        panel.appendBuildLine("util.cpp:42:8: warning: unused variable 'y' [-Wunused-variable]");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("util.cpp"));
        QCOMPARE(probs[0].line, 42);
        QCOMPARE(probs[0].column, 8);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Warning);
    }

    // ── GCC no column ─────────────────────────────────────────────────────

    void gccErrorNoColumn()
    {
        OutputPanel panel;
        panel.appendBuildLine("linker.cpp:99: error: undefined reference to 'foo'");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("linker.cpp"));
        QCOMPARE(probs[0].line, 99);
        QCOMPARE(probs[0].column, 1);  // default column = 1
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Error);
    }

    // ── MSVC errors ───────────────────────────────────────────────────────

    void msvcError()
    {
        OutputPanel panel;
        panel.appendBuildLine("main.cpp(25): error C2065: 'x': undeclared identifier");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("main.cpp"));
        QCOMPARE(probs[0].line, 25);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Error);
        QVERIFY(probs[0].message.contains("undeclared"));
    }

    void msvcWarning()
    {
        OutputPanel panel;
        panel.appendBuildLine("util.cpp(10): warning C4101: 'y': unreferenced local variable");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Warning);
    }

    // ── Rust / Cargo ──────────────────────────────────────────────────────

    void rustCargoError()
    {
        OutputPanel panel;
        panel.appendBuildLine("  --> src/main.rs:15:9");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("src/main.rs"));
        QCOMPARE(probs[0].line, 15);
        QCOMPARE(probs[0].column, 9);
    }

    // ── TypeScript / ESLint ───────────────────────────────────────────────

    void typescriptError()
    {
        OutputPanel panel;
        panel.appendBuildLine("app.ts(5,12): error TS2304: Cannot find name 'foo'");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("app.ts"));
        QCOMPARE(probs[0].line, 5);
        QCOMPARE(probs[0].column, 12);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Error);
        QVERIFY(probs[0].message.contains("Cannot find name"));
    }

    // ── Python traceback ──────────────────────────────────────────────────

    void pythonTraceback()
    {
        OutputPanel panel;
        panel.appendBuildLine("  File \"script.py\", line 42");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].file, QStringLiteral("script.py"));
        QCOMPARE(probs[0].line, 42);
    }

    // ── Generic file:line:message ─────────────────────────────────────────

    void genericFileLine()
    {
        OutputPanel panel;
        panel.appendBuildLine("config.yaml:7: duplicate key");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        QCOMPARE(probs[0].severity, OutputPanel::ProblemMatch::Info);
        QCOMPARE(probs[0].file, QStringLiteral("config.yaml"));
        QCOMPARE(probs[0].line, 7);
    }

    // ── No match (clean output) ───────────────────────────────────────────

    void cleanLineNoMatch()
    {
        OutputPanel panel;
        panel.appendBuildLine("Build complete. 0 errors, 0 warnings.");
        panel.appendBuildLine("[100%] Building CXX object...");
        panel.appendBuildLine("");

        QCOMPARE(panel.problems().size(), 0);
    }

    // ── Multiple problems in sequence ─────────────────────────────────────

    void multipleProblemsParsed()
    {
        OutputPanel panel;
        panel.appendBuildLine("a.cpp:1:1: error: fail 1");
        panel.appendBuildLine("b.cpp:2:3: warning: warn 1");
        panel.appendBuildLine("c.cpp(10): error C2001: oops");

        QCOMPARE(panel.problems().size(), 3);
        QCOMPARE(panel.problems()[0].severity, OutputPanel::ProblemMatch::Error);
        QCOMPARE(panel.problems()[1].severity, OutputPanel::ProblemMatch::Warning);
        QCOMPARE(panel.problems()[2].severity, OutputPanel::ProblemMatch::Error);
    }

    // ── problemsChanged signal ────────────────────────────────────────────

    void problemsChangedSignal()
    {
        OutputPanel panel;
        QSignalSpy spy(&panel, &OutputPanel::problemsChanged);

        panel.appendBuildLine("foo.cpp:1:1: error: broken");

        QCOMPARE(spy.count(), 1);
    }

    // ── Clear resets problems ─────────────────────────────────────────────

    void clearResetsProblemList()
    {
        OutputPanel panel;
        panel.appendBuildLine("x.cpp:1:1: error: bad");
        QCOMPARE(panel.problems().size(), 1);

        panel.clear();
        QCOMPARE(panel.problems().size(), 0);
    }

    // ── Relative path resolution ──────────────────────────────────────────

    void relativePathResolvedWithWorkDir()
    {
        OutputPanel panel;
        panel.setWorkingDirectory("/project");
        panel.appendBuildLine("src/main.cpp:5:1: error: oops");

        const auto probs = panel.problems();
        QCOMPARE(probs.size(), 1);
        // Should resolve relative path against workDir
        QVERIFY(probs[0].file.contains("project"));
        QVERIFY(probs[0].file.contains("src/main.cpp") ||
                probs[0].file.contains("src\\main.cpp"));
    }

    // ── Task profile JSON round-trip ──────────────────────────────────────

    void taskProfileSaveLoad()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString jsonPath = tmpDir.path() + "/tasks.json";

        OutputPanel panel;
        QList<TaskProfile> tasks;
        tasks.append({QStringLiteral("Build"), QStringLiteral("cmake"),
                      {"--build", "."}, QStringLiteral("/proj"), {}, true});
        tasks.append({QStringLiteral("Test"), QStringLiteral("ctest"),
                      {}, QStringLiteral("/proj"), {}, false});
        panel.setTaskProfiles(tasks);
        panel.saveTasksToJson(jsonPath);

        QVERIFY(QFile::exists(jsonPath));

        OutputPanel panel2;
        panel2.loadTasksFromJson(jsonPath);

        const auto loaded = panel2.taskProfiles();
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].label, QStringLiteral("Build"));
        QCOMPARE(loaded[0].command, QStringLiteral("cmake"));
        QCOMPARE(loaded[0].args, QStringList({"--build", "."}));
        QVERIFY(loaded[0].isDefault);
        QCOMPARE(loaded[1].label, QStringLiteral("Test"));
    }
};

QTEST_MAIN(TestOutputPanel)
#include "test_outputpanel.moc"
