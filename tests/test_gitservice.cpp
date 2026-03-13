#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QDir>

#include "git/gitservice.h"

class TestGitService : public QObject
{
    Q_OBJECT

private:
    static QString createGitRepo(QTemporaryDir &tmpDir)
    {
        if (!tmpDir.isValid())
            return {};

        const QString root = tmpDir.path();

        if (!runGit(root, {"init"}))
            return {};
        if (!runGit(root, {"config", "user.email", "test@test.com"}))
            return {};
        if (!runGit(root, {"config", "user.name", "Test"}))
            return {};

        writeFile(root + "/hello.txt", "Hello World\n");
        if (!runGit(root, {"add", "."}))
            return {};
        if (!runGit(root, {"commit", "-m", "initial"}))
            return {};

        return root;
    }

    static bool runGit(const QString &workDir, const QStringList &args)
    {
        QProcess proc;
        proc.setWorkingDirectory(workDir);
        proc.start("git", args);
        proc.waitForFinished(10000);
        return proc.exitCode() == 0;
    }

    static void writeFile(const QString &path, const QString &content)
    {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(content.toUtf8());
    }

    static void waitForRefresh(GitService &svc)
    {
        QSignalSpy spy(&svc, &GitService::statusRefreshed);
        for (int i = 0; i < 100 && spy.isEmpty(); ++i)
            QTest::qWait(50);
    }

private slots:

    void detectsGitRepo()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);
        QVERIFY(svc.isGitRepo());
    }

    void nonGitDir_notRepo()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GitService svc;
        svc.setWorkingDirectory(tmpDir.path());
        waitForRefresh(svc);
        QVERIFY(!svc.isGitRepo());
    }

    void currentBranch_mainOrMaster()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        const QString branch = svc.currentBranch();
        QVERIFY(branch == "main" || branch == "master");
    }

    void localBranches()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(!svc.localBranches().isEmpty());
    }

    void fileStatuses_clean()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(svc.fileStatuses().isEmpty());
    }

    void fileStatuses_modified()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/hello.txt", "Modified content\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(!svc.fileStatuses().isEmpty());
    }

    void fileStatuses_untracked()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/newfile.txt", "new\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        auto statuses = svc.fileStatuses();
        bool hasUntracked = false;
        for (auto it = statuses.begin(); it != statuses.end(); ++it) {
            if (it.value().contains("?"))
                hasUntracked = true;
        }
        QVERIFY(hasUntracked);
    }

    void diff_modified()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/hello.txt", "Changed\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(!svc.diff().isEmpty());
    }

    void diff_clean_empty()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(svc.diff().isEmpty());
    }

    void stageAndUnstage()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/hello.txt", "Staged content\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(svc.stageFile(root + "/hello.txt"));
        waitForRefresh(svc);
        QVERIFY(svc.unstageFile(root + "/hello.txt"));
        waitForRefresh(svc);
    }

    void commitChanges()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/hello.txt", "Committed content\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        svc.stageFile(root + "/hello.txt");
        waitForRefresh(svc);
        QVERIFY(svc.commit("test commit"));
        waitForRefresh(svc);
    }

    void createAndCheckoutBranch()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(svc.createBranch("feature-test"));
        waitForRefresh(svc);
        QCOMPARE(svc.currentBranch(), QStringLiteral("feature-test"));

        auto branches = svc.localBranches();
        QString mainBranch;
        for (const auto &b : branches) {
            if (b == "main" || b == "master") {
                mainBranch = b;
                break;
            }
        }
        QVERIFY(!mainBranch.isEmpty());
        QVERIFY(svc.checkoutBranch(mainBranch));
        waitForRefresh(svc);
        QCOMPARE(svc.currentBranch(), mainBranch);
    }

    void showAtHead()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        const QString content = svc.showAtHead(root + "/hello.txt");
        QCOMPARE(content.trimmed(), QStringLiteral("Hello World"));
    }

    void showAtHead_untracked_empty()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        writeFile(root + "/brand_new.txt", "new\n");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QVERIFY(svc.showAtHead(root + "/brand_new.txt").isEmpty());
    }

    void blame_returnsEntries()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        auto entries = svc.blame(root + "/hello.txt");
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].author, QStringLiteral("Test"));
    }

    void statusChar_clean()
    {
        QTemporaryDir tmpDir;
        const QString root = createGitRepo(tmpDir);
        if (root.isEmpty()) QSKIP("git not available");

        GitService svc;
        svc.setWorkingDirectory(root);
        waitForRefresh(svc);

        QChar ch = svc.statusChar(root + "/hello.txt");
        QVERIFY(ch.isNull() || ch == ' ');
    }
};

QTEST_MAIN(TestGitService)
#include "test_gitservice.moc"
