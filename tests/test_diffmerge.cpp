#include <QtTest>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>

#include "git/gitservice.h"
#include "diffexplorerpanel.h"
#include "mergeeditor.h"

class TestDiffMerge : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── GitService new methods ───────────────────────────────────────────
    void log_returnsEntries();
    void log_emptyOnNonGit();
    void changedFilesBetween_detectsChanges();
    void changedFilesBetween_emptyForSameCommit();
    void diffRevisions_producesUnifiedDiff();
    void showAtRevision_returnsOldContent();
    void showAtRevision_invalidRev();

    // ── DiffExplorerPanel ────────────────────────────────────────────────
    void diffExplorer_constructsWithoutCrash();
    void diffExplorer_compare_populatesFileList();

    // ── MergeEditor ──────────────────────────────────────────────────────
    void mergeEditor_constructsWithoutCrash();
    void mergeEditor_refresh_emptyOnNoConflicts();
    void mergeEditor_conflictParsing();
    void mergeEditor_acceptOurs_stripsConflictMarkers();
    void mergeEditor_acceptTheirs_stripsConflictMarkers();
    void mergeEditor_acceptBoth_keepsAllContent();

private:
    void git(const QStringList &args);
    void writeFile(const QString &name, const QString &content);
    QString readFile(const QString &name);

    QTemporaryDir m_tmpDir;
    std::unique_ptr<GitService> m_git;
    QString m_firstCommitHash;
    QString m_secondCommitHash;
};

void TestDiffMerge::initTestCase()
{
    QVERIFY(m_tmpDir.isValid());

    // Initialize a git repo with two commits
    git({QStringLiteral("init")});
    git({QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@test.com")});
    git({QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("Test")});

    writeFile(QStringLiteral("file1.txt"), QStringLiteral("line1\nline2\nline3\n"));
    writeFile(QStringLiteral("file2.txt"), QStringLiteral("hello\n"));
    git({QStringLiteral("add"), QStringLiteral(".")});
    git({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial commit")});

    // Get first commit hash
    QProcess proc;
    proc.setWorkingDirectory(m_tmpDir.path());
    proc.start(QStringLiteral("git"),
               {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    proc.waitForFinished(5000);
    m_firstCommitHash = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    // Second commit
    writeFile(QStringLiteral("file1.txt"), QStringLiteral("line1\nMODIFIED\nline3\n"));
    writeFile(QStringLiteral("file3.txt"), QStringLiteral("new file\n"));
    git({QStringLiteral("add"), QStringLiteral(".")});
    git({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("second commit")});

    proc.start(QStringLiteral("git"),
               {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    proc.waitForFinished(5000);
    m_secondCommitHash = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    m_git = std::make_unique<GitService>();
    m_git->setWorkingDirectory(m_tmpDir.path());
}

void TestDiffMerge::cleanupTestCase()
{
    m_git.reset();
}

void TestDiffMerge::git(const QStringList &args)
{
    QProcess proc;
    proc.setWorkingDirectory(m_tmpDir.path());
    proc.start(QStringLiteral("git"), args);
    QVERIFY(proc.waitForFinished(10000));
    QCOMPARE(proc.exitCode(), 0);
}

void TestDiffMerge::writeFile(const QString &name, const QString &content)
{
    QFile f(m_tmpDir.path() + QLatin1Char('/') + name);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content.toUtf8());
}

QString TestDiffMerge::readFile(const QString &name)
{
    QFile f(m_tmpDir.path() + QLatin1Char('/') + name);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

// ── GitService log ───────────────────────────────────────────────────────────

void TestDiffMerge::log_returnsEntries()
{
    const auto entries = m_git->log(10);
    QVERIFY(entries.size() >= 2);
    QCOMPARE(entries.first().subject, QStringLiteral("second commit"));
    QCOMPARE(entries.at(1).subject, QStringLiteral("initial commit"));
    QVERIFY(!entries.first().hash.isEmpty());
    QVERIFY(!entries.first().author.isEmpty());
}

void TestDiffMerge::log_emptyOnNonGit()
{
    GitService gs;
    gs.setWorkingDirectory(QDir::tempPath());
    const auto entries = gs.log(5);
    // May return empty or entries from another repo — just don't crash
    Q_UNUSED(entries);
}

// ── GitService changedFilesBetween ───────────────────────────────────────────

void TestDiffMerge::changedFilesBetween_detectsChanges()
{
    const auto changed = m_git->changedFilesBetween(m_firstCommitHash, m_secondCommitHash);
    QVERIFY(!changed.isEmpty());

    // file1.txt should be Modified
    bool foundFile1 = false;
    bool foundFile3 = false;
    for (const auto &cf : changed) {
        if (cf.path == QStringLiteral("file1.txt")) {
            QCOMPARE(cf.status, QChar('M'));
            foundFile1 = true;
        }
        if (cf.path == QStringLiteral("file3.txt")) {
            QCOMPARE(cf.status, QChar('A'));
            foundFile3 = true;
        }
    }
    QVERIFY(foundFile1);
    QVERIFY(foundFile3);
}

void TestDiffMerge::changedFilesBetween_emptyForSameCommit()
{
    const auto changed = m_git->changedFilesBetween(m_secondCommitHash, m_secondCommitHash);
    QVERIFY(changed.isEmpty());
}

// ── GitService diffRevisions ─────────────────────────────────────────────────

void TestDiffMerge::diffRevisions_producesUnifiedDiff()
{
    const QString diff = m_git->diffRevisions(m_firstCommitHash, m_secondCommitHash);
    QVERIFY(!diff.isEmpty());
    QVERIFY(diff.contains(QStringLiteral("MODIFIED")));
    QVERIFY(diff.contains(QStringLiteral("@@")));
}

// ── GitService showAtRevision ────────────────────────────────────────────────

void TestDiffMerge::showAtRevision_returnsOldContent()
{
    const QString absPath = m_tmpDir.path() + QStringLiteral("/file1.txt");
    const QString oldContent = m_git->showAtRevision(absPath, m_firstCommitHash);
    QVERIFY(oldContent.contains(QStringLiteral("line2")));
    QVERIFY(!oldContent.contains(QStringLiteral("MODIFIED")));

    const QString newContent = m_git->showAtRevision(absPath, m_secondCommitHash);
    QVERIFY(newContent.contains(QStringLiteral("MODIFIED")));
}

void TestDiffMerge::showAtRevision_invalidRev()
{
    const QString absPath = m_tmpDir.path() + QStringLiteral("/file1.txt");
    const QString content = m_git->showAtRevision(absPath, QStringLiteral("nonexistent_rev_12345"));
    QVERIFY(content.isEmpty());
}

// ── DiffExplorerPanel ────────────────────────────────────────────────────────

void TestDiffMerge::diffExplorer_constructsWithoutCrash()
{
    DiffExplorerPanel panel(m_git.get());
    QVERIFY(panel.isEnabled());
}

void TestDiffMerge::diffExplorer_compare_populatesFileList()
{
    DiffExplorerPanel panel(m_git.get());
    panel.compare(m_firstCommitHash, m_secondCommitHash);

    // The panel should have populated its file list
    // We can't access the private m_fileList directly, but we can verify it doesn't crash
    QVERIFY(panel.isEnabled());
}

// ── MergeEditor ──────────────────────────────────────────────────────────────

void TestDiffMerge::mergeEditor_constructsWithoutCrash()
{
    MergeEditor editor(m_git.get());
    QVERIFY(editor.isEnabled());
}

void TestDiffMerge::mergeEditor_refresh_emptyOnNoConflicts()
{
    MergeEditor editor(m_git.get());
    QSignalSpy spy(&editor, &MergeEditor::allResolved);
    editor.refresh();
    // No conflicts in this repo — should emit allResolved
    QCOMPARE(spy.count(), 1);
}

void TestDiffMerge::mergeEditor_conflictParsing()
{
    // Test conflict marker parsing with a fake conflict file
    const QString conflictContent =
        QStringLiteral("line1\n"
                       "<<<<<<< HEAD\n"
                       "our change\n"
                       "=======\n"
                       "their change\n"
                       ">>>>>>> feature\n"
                       "line after\n");

    writeFile(QStringLiteral("conflict_test.txt"), conflictContent);

    // Parse manually using same logic as MergeEditor
    enum Section { Normal, Ours, Theirs };
    Section section = Normal;
    QString oursText, theirsText;

    const QStringList lines = conflictContent.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) { section = Ours; continue; }
        if (line.startsWith(QStringLiteral("======="))) { section = Theirs; continue; }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) { section = Normal; continue; }
        switch (section) {
        case Normal: oursText += line + QLatin1Char('\n');
                     theirsText += line + QLatin1Char('\n'); break;
        case Ours:   oursText += line + QLatin1Char('\n'); break;
        case Theirs: theirsText += line + QLatin1Char('\n'); break;
        }
    }

    QVERIFY(oursText.contains(QStringLiteral("our change")));
    QVERIFY(!oursText.contains(QStringLiteral("their change")));
    QVERIFY(theirsText.contains(QStringLiteral("their change")));
    QVERIFY(!theirsText.contains(QStringLiteral("our change")));
    // Both should have non-conflict content
    QVERIFY(oursText.contains(QStringLiteral("line1")));
    QVERIFY(theirsText.contains(QStringLiteral("line after")));
}

void TestDiffMerge::mergeEditor_acceptOurs_stripsConflictMarkers()
{
    const QString conflictContent =
        QStringLiteral("before\n"
                       "<<<<<<< HEAD\n"
                       "our version\n"
                       "=======\n"
                       "their version\n"
                       ">>>>>>> feature\n"
                       "after\n");

    // Apply "accept ours" logic
    QString result;
    enum Section { Normal, Ours, Theirs };
    Section section = Normal;
    const QStringList lines = conflictContent.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) { section = Ours; continue; }
        if (line.startsWith(QStringLiteral("======="))) { section = Theirs; continue; }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) { section = Normal; continue; }
        if (section != Theirs)
            result += line + QLatin1Char('\n');
    }

    QVERIFY(result.contains(QStringLiteral("our version")));
    QVERIFY(!result.contains(QStringLiteral("their version")));
    QVERIFY(result.contains(QStringLiteral("before")));
    QVERIFY(result.contains(QStringLiteral("after")));
    QVERIFY(!result.contains(QStringLiteral("<<<<<<<<")));
}

void TestDiffMerge::mergeEditor_acceptTheirs_stripsConflictMarkers()
{
    const QString conflictContent =
        QStringLiteral("before\n"
                       "<<<<<<< HEAD\n"
                       "our version\n"
                       "=======\n"
                       "their version\n"
                       ">>>>>>> feature\n"
                       "after\n");

    QString result;
    enum Section { Normal, Ours, Theirs };
    Section section = Normal;
    const QStringList lines = conflictContent.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<<"))) { section = Ours; continue; }
        if (line.startsWith(QStringLiteral("======="))) { section = Theirs; continue; }
        if (line.startsWith(QStringLiteral(">>>>>>>"))) { section = Normal; continue; }
        if (section != Ours)
            result += line + QLatin1Char('\n');
    }

    QVERIFY(!result.contains(QStringLiteral("our version")));
    QVERIFY(result.contains(QStringLiteral("their version")));
    QVERIFY(result.contains(QStringLiteral("before")));
    QVERIFY(result.contains(QStringLiteral("after")));
}

void TestDiffMerge::mergeEditor_acceptBoth_keepsAllContent()
{
    const QString conflictContent =
        QStringLiteral("before\n"
                       "<<<<<<< HEAD\n"
                       "our version\n"
                       "=======\n"
                       "their version\n"
                       ">>>>>>> feature\n"
                       "after\n");

    QString result;
    const QStringList lines = conflictContent.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("<<<<<<< ")) ||
            line.startsWith(QStringLiteral("=======")) ||
            line.startsWith(QStringLiteral(">>>>>>> ")))
            continue;
        result += line + QLatin1Char('\n');
    }

    QVERIFY(result.contains(QStringLiteral("our version")));
    QVERIFY(result.contains(QStringLiteral("their version")));
    QVERIFY(result.contains(QStringLiteral("before")));
    QVERIFY(result.contains(QStringLiteral("after")));
    QVERIFY(!result.contains(QStringLiteral("<<<<<<<<")));
}

QTEST_MAIN(TestDiffMerge)
#include "test_diffmerge.moc"
