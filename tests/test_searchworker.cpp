#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "search/searchworker.h"
#include "search/searchquery.h"
#include "search/searchmatch.h"

class TestSearchWorker : public QObject
{
    Q_OBJECT

private:
    /// Create a temp directory with test files and return its path.
    QString createSearchFixture()
    {
        if (!m_tmpDir.isValid())
            return {};

        const QString root = m_tmpDir.path();

        writeFile(root + "/hello.txt",
                  "Hello World\nfoo bar baz\nHello again\n");
        writeFile(root + "/code.cpp",
                  "#include <stdio.h>\nint main() {\n    printf(\"hello\");\n    return 0;\n}\n");
        writeFile(root + "/notes.md",
                  "# Notes\n- item one\n- item two\n- ITEM THREE\n");

        // Nested dir
        QDir(root).mkpath("sub");
        writeFile(root + "/sub/deep.txt",
                  "deep content line one\ndeep content line two\n");

        return root;
    }

    static void writeFile(const QString &path, const QString &content)
    {
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        f.write(content.toUtf8());
        f.close();
    }

    QTemporaryDir m_tmpDir;

private slots:

    // ── Literal search ────────────────────────────────────────────────────

    void literal_caseSensitive()
    {
        const QString root = createSearchFixture();
        QVERIFY(!root.isEmpty());

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);
        QSignalSpy finSpy(&worker, &SearchWorker::finished);

        SearchQuery q;
        q.pattern = "Hello";
        q.mode = SearchMode::Literal;
        q.caseSensitive = true;

        worker.run(root, q);

        QCOMPARE(finSpy.count(), 1);
        // "Hello World" and "Hello again" in hello.txt — but NOT "hello" in code.cpp
        int helloMatches = 0;
        for (const auto &args : matchSpy) {
            auto match = args.at(0).value<SearchMatch>();
            if (match.filePath.endsWith("hello.txt"))
                ++helloMatches;
            // "hello" in code.cpp should NOT match (different case)
            QVERIFY(!match.preview.contains("printf"));
        }
        QCOMPARE(helloMatches, 2);
    }

    void literal_caseInsensitive()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "hello";
        q.mode = SearchMode::Literal;
        q.caseSensitive = false;

        worker.run(root, q);

        // Should find "Hello" (x2 in hello.txt) + "hello" (x1 in code.cpp) = 3
        QVERIFY(matchSpy.count() >= 3);
    }

    // ── Regex search ──────────────────────────────────────────────────────

    void regex_basic()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "item \\w+";
        q.mode = SearchMode::Regex;
        q.caseSensitive = false;

        worker.run(root, q);

        // notes.md has "item one", "item two", "ITEM THREE" — all match case-insensitive
        QVERIFY(matchSpy.count() >= 3);
    }

    void regex_invalidPattern()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);
        QSignalSpy finSpy(&worker, &SearchWorker::finished);

        SearchQuery q;
        q.pattern = "[invalid";
        q.mode = SearchMode::Regex;

        worker.run(root, q);

        // Should gracefully finish with no matches
        QCOMPARE(finSpy.count(), 1);
        QCOMPARE(matchSpy.count(), 0);
    }

    // ── Whole word ────────────────────────────────────────────────────────

    void wholeWord_literal()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "item";
        q.mode = SearchMode::Literal;
        q.wholeWord = true;
        q.caseSensitive = false;

        worker.run(root, q);

        // "item one", "item two", "ITEM THREE" — but NOT "item" embedded in other words
        QVERIFY(matchSpy.count() >= 3);
    }

    // ── Match details ─────────────────────────────────────────────────────

    void matchHasCorrectLineAndColumn()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "printf";
        q.mode = SearchMode::Literal;
        q.caseSensitive = true;

        worker.run(root, q);

        QCOMPARE(matchSpy.count(), 1);
        auto match = matchSpy.at(0).at(0).value<SearchMatch>();
        QVERIFY(match.filePath.endsWith("code.cpp"));
        QCOMPARE(match.line, 3);  // 1-based: line 3 has printf
        QVERIFY(match.column > 0);
    }

    // ── Subdirectory search ───────────────────────────────────────────────

    void searchesSubdirectories()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "deep content";
        q.mode = SearchMode::Literal;
        q.caseSensitive = true;

        worker.run(root, q);

        QCOMPARE(matchSpy.count(), 2);
        for (const auto &args : matchSpy) {
            auto match = args.at(0).value<SearchMatch>();
            QVERIFY(match.filePath.contains("sub"));
        }
    }

    // ── Preview truncation ────────────────────────────────────────────────

    void previewTruncatedForLongLines()
    {
        const QString root = m_tmpDir.path();
        // Create a file with a very long line
        QString longLine = QString("match_here").leftJustified(300, 'x');
        writeFile(root + "/long.txt", longLine + "\n");

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "match_here";
        q.mode = SearchMode::Literal;

        worker.run(root, q);

        QVERIFY(matchSpy.count() >= 1);
        auto match = matchSpy.last().at(0).value<SearchMatch>();
        // Preview should be capped at 200 chars + "..."
        QVERIFY(match.preview.length() <= 204);
    }

    // ── Empty results ─────────────────────────────────────────────────────

    void noMatchesEmitsFinished()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);
        QSignalSpy finSpy(&worker, &SearchWorker::finished);

        SearchQuery q;
        q.pattern = "zzz_definitely_not_found_zzz";
        q.mode = SearchMode::Literal;

        worker.run(root, q);

        QCOMPARE(finSpy.count(), 1);
        QCOMPARE(matchSpy.count(), 0);
    }

    // ── Cancellation ──────────────────────────────────────────────────────

    void cancelStopsSearch()
    {
        const QString root = createSearchFixture();

        SearchWorker worker;
        // Cancel immediately before running
        worker.requestCancel();

        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);
        QSignalSpy finSpy(&worker, &SearchWorker::finished);

        SearchQuery q;
        q.pattern = "Hello";
        q.mode = SearchMode::Literal;

        worker.run(root, q);

        QCOMPARE(finSpy.count(), 1);
        // Note: cancellation is checked at file boundaries,
        // so we might still get some matches before cancel kicks in.
        // This just ensures it finishes without crashing.
    }

    // ── Large file skip ───────────────────────────────────────────────────

    void skipsLargeFiles()
    {
        const QString root = m_tmpDir.path();

        // Create a file just over 1MB
        QString bigContent(1024 * 1024 + 100, 'a');
        bigContent.replace(500000, 6, "MARKER");
        writeFile(root + "/big.txt", bigContent);

        SearchWorker worker;
        QSignalSpy matchSpy(&worker, &SearchWorker::matchFound);

        SearchQuery q;
        q.pattern = "MARKER";
        q.mode = SearchMode::Literal;
        q.caseSensitive = true;

        worker.run(root, q);

        // MARKER is in a file > 1MB — should be skipped
        bool foundInBig = false;
        for (const auto &args : matchSpy) {
            auto match = args.at(0).value<SearchMatch>();
            if (match.filePath.endsWith("big.txt"))
                foundInBig = true;
        }
        QVERIFY(!foundInBig);
    }
};

QTEST_MAIN(TestSearchWorker)
#include "test_searchworker.moc"
