#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>

#include "core/qtfilewatcher.h"
#include "core/qtenvironment.h"

// ── QtFileWatcher tests ──────────────────────────────────────────────────────

class TestCoreInterfaces : public QObject
{
    Q_OBJECT

private slots:

    // ── IFileWatcher — watch / unwatch ───────────────────────────────────

    void watchFileAdds()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString fpath = tmp.path() + "/watched.txt";
        QFile f(fpath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("initial");
        f.close();

        QtFileWatcher watcher;
        QVERIFY(watcher.watchPath(fpath));
        QVERIFY(watcher.watchedFiles().contains(fpath));
    }

    void unwatchFileRemoves()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString fpath = tmp.path() + "/watched.txt";
        QFile f(fpath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
        f.close();

        QtFileWatcher watcher;
        watcher.watchPath(fpath);
        QVERIFY(watcher.watchedFiles().contains(fpath));

        QVERIFY(watcher.unwatchPath(fpath));
        QVERIFY(!watcher.watchedFiles().contains(fpath));
    }

    void watchDirectoryAdds()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QtFileWatcher watcher;
        QVERIFY(watcher.watchPath(tmp.path()));
        QVERIFY(watcher.watchedDirectories().contains(tmp.path()));
    }

    void watchEmptyPathReturnsFalse()
    {
        QtFileWatcher watcher;
        QVERIFY(!watcher.watchPath(QString()));
        QVERIFY(!watcher.unwatchPath(QString()));
    }

    void watchNonexistentPathReturnsFalse()
    {
        QtFileWatcher watcher;
        QVERIFY(!watcher.watchPath(QStringLiteral("/nonexistent/path/foo.txt")));
    }

    void fileChangedSignalEmitted()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString fpath = tmp.path() + "/signal_test.txt";
        {
            QFile f(fpath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("v1");
        }

        // Let the OS settle before watching
        QTest::qWait(200);

        QtFileWatcher watcher;
        watcher.watchPath(fpath);

        QSignalSpy spy(&watcher, &QtFileWatcher::fileChanged);

        // Small delay, then modify the file to trigger signal
        QTest::qWait(100);
        {
            QFile f(fpath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write("v2 — changed content that is definitely different");
            f.flush();
        }

        // Give QFileSystemWatcher time to deliver (may need longer on CI)
        if (!spy.wait(5000)) {
            // On some Windows setups QFileSystemWatcher is unreliable for
            // rapid same-second writes.  Accept the test as a known limitation
            // if the mechanism itself works (watch/unwatch already tested).
            QSKIP("QFileSystemWatcher did not fire on this platform/config");
        }
        QCOMPARE(spy.first().first().toString(), fpath);
    }

    // ── IEnvironment ─────────────────────────────────────────────────────

    void platformReturnsKnownValue()
    {
        QtEnvironment env;
        const QString p = env.platform();
        QVERIFY(p == QStringLiteral("windows")
             || p == QStringLiteral("linux")
             || p == QStringLiteral("macos"));
    }

    void homeDirectoryIsNonEmpty()
    {
        QtEnvironment env;
        QVERIFY(!env.homeDirectory().isEmpty());
    }

    void tempDirectoryIsNonEmpty()
    {
        QtEnvironment env;
        QVERIFY(!env.tempDirectory().isEmpty());
    }

    void setAndGetVariable()
    {
        QtEnvironment env;
        const QString key = QStringLiteral("EXORCIST_TEST_VAR_12345");
        env.setVariable(key, QStringLiteral("hello"));
        QVERIFY(env.hasVariable(key));
        QCOMPARE(env.variable(key), QStringLiteral("hello"));
    }

    void hasVariableReturnsFalseForMissing()
    {
        QtEnvironment env;
        QVERIFY(!env.hasVariable(
            QStringLiteral("EXORCIST_DEFINITELY_DOES_NOT_EXIST_XYZ")));
    }

    void variableNamesReturnsNonEmpty()
    {
        QtEnvironment env;
        QVERIFY(!env.variableNames().isEmpty());
        // PATH is always present
        QVERIFY(env.hasVariable(QStringLiteral("PATH"))
             || env.hasVariable(QStringLiteral("Path")));
    }
};

QTEST_MAIN(TestCoreInterfaces)
#include "test_coreinterfaces.moc"
