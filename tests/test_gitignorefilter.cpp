#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "search/gitignorefilter.h"

class TestGitignoreFilter : public QObject
{
    Q_OBJECT

private slots:
    // ── Construction ──────────────────────────────────────────────────────

    void defaultEmpty()
    {
        GitignoreFilter filter;
        QCOMPARE(filter.patternCount(), 0);
        QVERIFY(!filter.isIgnored(QStringLiteral("anything.txt")));
    }

    // ── Simple wildcard patterns ──────────────────────────────────────────

    void simpleWildcard_star()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        QVERIFY(filter.isIgnored(QStringLiteral("debug.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("error.log")));
        QVERIFY(!filter.isIgnored(QStringLiteral("main.cpp")));
    }

    void simpleWildcard_starInSubdir()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        QVERIFY(filter.isIgnored(QStringLiteral("logs/debug.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("deep/nested/error.log")));
    }

    void simpleWildcard_question()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("?.txt"));
        QVERIFY(filter.isIgnored(QStringLiteral("a.txt")));
        QVERIFY(!filter.isIgnored(QStringLiteral("ab.txt")));
    }

    // ── Exact name patterns ──────────────────────────────────────────────

    void exactName()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("node_modules"));
        QVERIFY(filter.isIgnored(QStringLiteral("node_modules")));
        QVERIFY(filter.isIgnored(QStringLiteral("project/node_modules")));
        QVERIFY(!filter.isIgnored(QStringLiteral("src/main.cpp")));
    }

    // ── Directory-only patterns (trailing /) ─────────────────────────────

    void dirOnlyPattern_ignoredIfDir()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("build/"));
        QVERIFY(filter.isIgnored(QStringLiteral("build"), true));
        QVERIFY(!filter.isIgnored(QStringLiteral("build"), false));
    }

    void dirOnlyPattern_doesNotMatchFile()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("logs/"));
        QVERIFY(!filter.isIgnored(QStringLiteral("logs")));  // not flagged as dir
        QVERIFY(filter.isIgnored(QStringLiteral("logs"), true));
    }

    // ── Negation patterns (!) ────────────────────────────────────────────

    void negationPattern_overridesIgnore()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        filter.addPattern(QStringLiteral("!important.log"));
        QVERIFY(!filter.isIgnored(QStringLiteral("important.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("debug.log")));
    }

    void negationPattern_orderMatters()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("!keep.txt"));
        filter.addPattern(QStringLiteral("*.txt"));
        // Last matching pattern wins — *.txt ignores all .txt
        QVERIFY(filter.isIgnored(QStringLiteral("keep.txt")));
    }

    // ── Double-star (**) patterns ────────────────────────────────────────

    void doubleStar_matchesEverything()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("**/debug.log"));
        QVERIFY(filter.isIgnored(QStringLiteral("debug.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("src/debug.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("a/b/c/debug.log")));
        QVERIFY(!filter.isIgnored(QStringLiteral("release.log")));
    }

    void doubleStar_trailingMatch()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("logs/**"));
        QVERIFY(filter.isIgnored(QStringLiteral("logs/app.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("logs/2024/jan.log")));
    }

    // ── Path-containing patterns ─────────────────────────────────────────

    void pathPattern_matchesSubdir()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("build-*/"));
        QVERIFY(filter.isIgnored(QStringLiteral("build-llvm"), true));
        QVERIFY(filter.isIgnored(QStringLiteral("build-release"), true));
        QVERIFY(!filter.isIgnored(QStringLiteral("src"), true));
    }

    // ── Character class [abc] ────────────────────────────────────────────

    void characterClass()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.[oa]"));
        QVERIFY(filter.isIgnored(QStringLiteral("main.o")));
        QVERIFY(filter.isIgnored(QStringLiteral("lib.a")));
        QVERIFY(!filter.isIgnored(QStringLiteral("main.cpp")));
    }

    // ── addExcludePatterns ───────────────────────────────────────────────

    void addExcludePatterns_bulk()
    {
        GitignoreFilter filter;
        filter.addExcludePatterns({
            QStringLiteral("*.tmp"),
            QStringLiteral("*.bak"),
            QStringLiteral("*.swp")
        });
        QCOMPARE(filter.patternCount(), 3);
        QVERIFY(filter.isIgnored(QStringLiteral("file.tmp")));
        QVERIFY(filter.isIgnored(QStringLiteral("file.bak")));
        QVERIFY(filter.isIgnored(QStringLiteral("file.swp")));
        QVERIFY(!filter.isIgnored(QStringLiteral("file.cpp")));
    }

    // ── filterPaths ──────────────────────────────────────────────────────

    void filterPaths_removesIgnored()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));

        QStringList paths = {
            QStringLiteral("/root/src/main.cpp"),
            QStringLiteral("/root/debug.log"),
            QStringLiteral("/root/error.log"),
            QStringLiteral("/root/build/app.exe"),
        };

        QStringList result = filter.filterPaths(paths, QStringLiteral("/root"));
        QCOMPARE(result.size(), 2);
        QVERIFY(result.contains(QStringLiteral("/root/src/main.cpp")));
        QVERIFY(result.contains(QStringLiteral("/root/build/app.exe")));
    }

    void filterPaths_emptyInput()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        QStringList result = filter.filterPaths({}, QStringLiteral("/root"));
        QVERIFY(result.isEmpty());
    }

    // ── clear ────────────────────────────────────────────────────────────

    void clear_removesAllPatterns()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        filter.addPattern(QStringLiteral("*.tmp"));
        QCOMPARE(filter.patternCount(), 2);
        filter.clear();
        QCOMPARE(filter.patternCount(), 0);
        QVERIFY(!filter.isIgnored(QStringLiteral("test.log")));
    }

    // ── loadFile ─────────────────────────────────────────────────────────

    void loadFile_parsesGitignore()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QFile f(tmp.path() + QStringLiteral("/.gitignore"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Comment line\n"
                "*.log\n"
                "build/\n"
                "\n"
                "!important.log\n");
        f.close();

        GitignoreFilter filter;
        filter.loadFile(tmp.path() + QStringLiteral("/.gitignore"));

        QCOMPARE(filter.patternCount(), 3);  // comment and blank skipped
        QVERIFY(filter.isIgnored(QStringLiteral("debug.log")));
        QVERIFY(!filter.isIgnored(QStringLiteral("important.log")));
        QVERIFY(filter.isIgnored(QStringLiteral("build"), true));
    }

    void loadFile_nonexistent()
    {
        GitignoreFilter filter;
        filter.loadFile(QStringLiteral("/nonexistent/.gitignore"));
        QCOMPARE(filter.patternCount(), 0);
    }

    // ── Multiple patterns interaction ────────────────────────────────────

    void multiplePatterns_lastWins()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.txt"));
        filter.addPattern(QStringLiteral("!readme.txt"));
        filter.addPattern(QStringLiteral("readme.txt"));
        // *.txt → ignored, !readme.txt → kept, readme.txt → ignored again
        QVERIFY(filter.isIgnored(QStringLiteral("readme.txt")));
    }

    // ── Edge cases ───────────────────────────────────────────────────────

    void emptyRelativePath()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral("*.log"));
        QVERIFY(!filter.isIgnored(QString()));
    }

    void dotfiles()
    {
        GitignoreFilter filter;
        filter.addPattern(QStringLiteral(".env"));
        QVERIFY(filter.isIgnored(QStringLiteral(".env")));
        QVERIFY(filter.isIgnored(QStringLiteral("project/.env")));
    }
};

QTEST_MAIN(TestGitignoreFilter)
#include "test_gitignorefilter.moc"
