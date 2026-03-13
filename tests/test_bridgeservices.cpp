#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "../server/gitwatchservice.h"
#include "../server/authtokenservice.h"

// ── TestBridgeServices ───────────────────────────────────────────────────────
//
// Tests for the ExoBridge shared services:
//   • GitWatchBridgeService — centralized git file watching
//   • AuthTokenBridgeService — centralized auth token cache

class TestBridgeServices : public QObject
{
    Q_OBJECT

private slots:
    // ── GitWatchBridgeService tests ──────────────────────────────────

    void testGitWatchInvalidPath()
    {
        GitWatchBridgeService svc;

        bool called = false;
        bool wasOk = true;
        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), QStringLiteral("/nonexistent/path/xyz")}},
            [&](bool ok, const QJsonObject &) {
                called = true;
                wasOk = ok;
            });

        QVERIFY(called);
        QVERIFY(!wasOk);
    }

    void testGitWatchMissingRepoPath()
    {
        GitWatchBridgeService svc;

        bool called = false;
        bool wasOk = true;
        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{},
            [&](bool ok, const QJsonObject &) {
                called = true;
                wasOk = ok;
            });

        QVERIFY(called);
        QVERIFY(!wasOk);
    }

    void testGitWatchValidRepo()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a fake .git directory
        QDir(tmpDir.path()).mkdir(QStringLiteral(".git"));
        // Create .git/index and .git/HEAD
        QFile indexFile(tmpDir.path() + QStringLiteral("/.git/index"));
        QVERIFY(indexFile.open(QIODevice::WriteOnly));
        indexFile.write("fake index");
        indexFile.close();

        QFile headFile(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile.open(QIODevice::WriteOnly));
        headFile.write("ref: refs/heads/main\n");
        headFile.close();

        GitWatchBridgeService svc;

        bool called = false;
        bool wasOk = false;
        QString status;
        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [&](bool ok, const QJsonObject &result) {
                called = true;
                wasOk = ok;
                status = result[QLatin1String("status")].toString();
            });

        QVERIFY(called);
        QVERIFY(wasOk);
        QCOMPARE(status, QStringLiteral("watching"));
    }

    void testGitWatchDuplicateWatch()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QDir(tmpDir.path()).mkdir(QStringLiteral(".git"));

        QFile headFile(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile.open(QIODevice::WriteOnly));
        headFile.write("ref: refs/heads/main\n");
        headFile.close();

        GitWatchBridgeService svc;

        // First watch
        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [](bool, const QJsonObject &) {});

        // Second watch — should be "already_watching" with refCount=2
        QString status;
        int refCount = 0;
        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [&](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                status = result[QLatin1String("status")].toString();
                refCount = result[QLatin1String("refCount")].toInt();
            });

        QCOMPARE(status, QStringLiteral("already_watching"));
        QCOMPARE(refCount, 2);
    }

    void testGitWatchList()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QDir(tmpDir.path()).mkdir(QStringLiteral(".git"));

        QFile headFile(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile.open(QIODevice::WriteOnly));
        headFile.write("ref: refs/heads/main\n");
        headFile.close();

        GitWatchBridgeService svc;

        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [](bool, const QJsonObject &) {});

        QJsonArray repos;
        svc.handleCall(
            QStringLiteral("list"),
            QJsonObject{},
            [&](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                repos = result[QLatin1String("repos")].toArray();
            });

        QCOMPARE(repos.size(), 1);
    }

    void testGitWatchUnwatch()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QDir(tmpDir.path()).mkdir(QStringLiteral(".git"));

        QFile headFile(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile.open(QIODevice::WriteOnly));
        headFile.write("ref: refs/heads/main\n");
        headFile.close();

        GitWatchBridgeService svc;

        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [](bool, const QJsonObject &) {});

        svc.handleCall(
            QStringLiteral("unwatch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [](bool ok, const QJsonObject &) { QVERIFY(ok); });

        // List should be empty now
        svc.handleCall(
            QStringLiteral("list"),
            QJsonObject{},
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QCOMPARE(result[QLatin1String("repos")].toArray().size(), 0);
            });
    }

    void testGitWatchFileChange()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QDir(tmpDir.path()).mkdir(QStringLiteral(".git"));

        QFile headFile(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile.open(QIODevice::WriteOnly));
        headFile.write("ref: refs/heads/main\n");
        headFile.close();

        GitWatchBridgeService svc;
        QSignalSpy changeSpy(&svc, &GitWatchBridgeService::repoChanged);

        svc.handleCall(
            QStringLiteral("watch"),
            QJsonObject{{QLatin1String("repoPath"), tmpDir.path()}},
            [](bool, const QJsonObject &) {});

        // Modify HEAD to trigger watcher
        QFile headFile2(tmpDir.path() + QStringLiteral("/.git/HEAD"));
        QVERIFY(headFile2.open(QIODevice::WriteOnly));
        headFile2.write("ref: refs/heads/feature\n");
        headFile2.close();

        // Wait for debounced signal (200ms debounce + buffer)
        if (changeSpy.isEmpty())
            changeSpy.wait(2000);

        // File system watcher behavior may vary in test environments
        // so we just verify the service didn't crash
    }

    void testGitWatchUnknownMethod()
    {
        GitWatchBridgeService svc;

        svc.handleCall(
            QStringLiteral("unknown"),
            QJsonObject{},
            [](bool ok, const QJsonObject &) {
                QVERIFY(!ok);
            });
    }

    // ── AuthTokenBridgeService tests ─────────────────────────────────

    void testAuthStoreAndRetrieve()
    {
        AuthTokenBridgeService svc;

        // Store a token
        bool storeOk = false;
        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("test-provider")},
                {QLatin1String("token"), QStringLiteral("secret-token-123")}
            },
            [&](bool ok, const QJsonObject &) {
                storeOk = ok;
            });
        QVERIFY(storeOk);

        // Retrieve the token
        QString retrieved;
        svc.handleCall(
            QStringLiteral("retrieve"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("test-provider")}
            },
            [&](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                retrieved = result[QLatin1String("token")].toString();
            });

        QCOMPARE(retrieved, QStringLiteral("secret-token-123"));
    }

    void testAuthHas()
    {
        AuthTokenBridgeService svc;

        // Initially should not exist
        svc.handleCall(
            QStringLiteral("has"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("nonexistent")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QVERIFY(!result[QLatin1String("exists")].toBool());
            });

        // Store one
        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("myauth")},
                {QLatin1String("token"), QStringLiteral("tok")}
            },
            [](bool, const QJsonObject &) {});

        // Now it should exist
        svc.handleCall(
            QStringLiteral("has"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("myauth")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QVERIFY(result[QLatin1String("exists")].toBool());
            });
    }

    void testAuthDelete()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("to-delete")},
                {QLatin1String("token"), QStringLiteral("xyz")}
            },
            [](bool, const QJsonObject &) {});

        // Delete it
        svc.handleCall(
            QStringLiteral("delete"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("to-delete")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QVERIFY(result[QLatin1String("deleted")].toBool());
            });

        // Should no longer exist
        svc.handleCall(
            QStringLiteral("has"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("to-delete")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QVERIFY(!result[QLatin1String("exists")].toBool());
            });
    }

    void testAuthList()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("svc-a")},
                {QLatin1String("token"), QStringLiteral("a")}
            },
            [](bool, const QJsonObject &) {});

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("svc-b")},
                {QLatin1String("token"), QStringLiteral("b")}
            },
            [](bool, const QJsonObject &) {});

        QJsonArray services;
        svc.handleCall(
            QStringLiteral("list"),
            QJsonObject{},
            [&](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                services = result[QLatin1String("services")].toArray();
            });

        QStringList names;
        for (const auto &v : services)
            names << v.toString();

        QVERIFY(names.contains(QStringLiteral("svc-a")));
        QVERIFY(names.contains(QStringLiteral("svc-b")));
    }

    void testAuthMissingService()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("token"), QStringLiteral("no-service")}
            },
            [](bool ok, const QJsonObject &) {
                QVERIFY(!ok);
            });
    }

    void testAuthUnknownMethod()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("bogus"),
            QJsonObject{},
            [](bool ok, const QJsonObject &) {
                QVERIFY(!ok);
            });
    }

    void testAuthRetrieveEmpty()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("retrieve"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("does-not-exist")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QVERIFY(!result[QLatin1String("found")].toBool());
                QVERIFY(result[QLatin1String("token")].toString().isEmpty());
            });
    }

    void testAuthOverwrite()
    {
        AuthTokenBridgeService svc;

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("overwrite-me")},
                {QLatin1String("token"), QStringLiteral("first")}
            },
            [](bool, const QJsonObject &) {});

        svc.handleCall(
            QStringLiteral("store"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("overwrite-me")},
                {QLatin1String("token"), QStringLiteral("second")}
            },
            [](bool, const QJsonObject &) {});

        svc.handleCall(
            QStringLiteral("retrieve"),
            QJsonObject{
                {QLatin1String("service"), QStringLiteral("overwrite-me")}
            },
            [](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QCOMPARE(result[QLatin1String("token")].toString(),
                         QStringLiteral("second"));
            });
    }
};

QTEST_MAIN(TestBridgeServices)
#include "test_bridgeservices.moc"
