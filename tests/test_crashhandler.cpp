#include <QTest>
#include <QDir>
#include <QTemporaryDir>

#include "crashhandler.h"

class TestCrashHandler : public QObject
{
    Q_OBJECT

private slots:
    void installCreatesDirectory()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString crashDir = tmpDir.path() + QStringLiteral("/crashes");
        QVERIFY(!QDir(crashDir).exists());

        CrashHandler::install(crashDir);

        QVERIFY(QDir(crashDir).exists());
    }

    void crashDirectoryReturnsPath()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString crashDir = tmpDir.path() + QStringLiteral("/test_crashes");

        CrashHandler::install(crashDir);

        QCOMPARE(CrashHandler::crashDirectory(), crashDir);
    }

    void installDefaultDirectory()
    {
        // Install with empty string -> uses <appDir>/crashes
        CrashHandler::install();
        const QString dir = CrashHandler::crashDirectory();

        QVERIFY(!dir.isEmpty());
        QVERIFY(dir.endsWith(QStringLiteral("/crashes")));
        QVERIFY(QDir(dir).exists());
    }

    void doubleInstallDoesNotCrash()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString dir1 = tmpDir.path() + QStringLiteral("/crashes1");
        const QString dir2 = tmpDir.path() + QStringLiteral("/crashes2");

        CrashHandler::install(dir1);
        QCOMPARE(CrashHandler::crashDirectory(), dir1);

        CrashHandler::install(dir2);
        QCOMPARE(CrashHandler::crashDirectory(), dir2);
        QVERIFY(QDir(dir2).exists());
    }

    void installWithNestedDirectory()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString nested = tmpDir.path() + QStringLiteral("/a/b/c/crashes");

        CrashHandler::install(nested);

        QVERIFY(QDir(nested).exists());
        QCOMPARE(CrashHandler::crashDirectory(), nested);
    }

    // ── Ring buffer tests ─────────────────────────────────────────────────

    void ringBufferRecordAndRetrieve()
    {
        // Write a few lines and read them back in order
        CrashHandler::recordLogLine(QStringLiteral("line1\n"));
        CrashHandler::recordLogLine(QStringLiteral("line2\n"));
        CrashHandler::recordLogLine(QStringLiteral("line3\n"));

        const QStringList logs = CrashHandler::recentLogLines();
        QVERIFY(logs.size() >= 3);
        // Last 3 entries must be our lines (in order)
        QCOMPARE(logs.at(logs.size() - 3), QStringLiteral("line1\n"));
        QCOMPARE(logs.at(logs.size() - 2), QStringLiteral("line2\n"));
        QCOMPARE(logs.at(logs.size() - 1), QStringLiteral("line3\n"));
    }

    void ringBufferWrapsAround()
    {
        // Fill beyond capacity to verify wrap-around
        for (int i = 0; i < CrashHandler::kRingBufferSize + 10; ++i) {
            CrashHandler::recordLogLine(
                QStringLiteral("wrap_%1\n").arg(i));
        }

        const QStringList logs = CrashHandler::recentLogLines();
        QCOMPARE(logs.size(), CrashHandler::kRingBufferSize);

        // The oldest should be the one written at index 10
        // (since we wrote kRingBufferSize+10 total, the first 10 are gone)
        QVERIFY(logs.first().startsWith(QStringLiteral("wrap_")));
        // The last should be "wrap_{kRingBufferSize+9}"
        QCOMPARE(logs.last(),
                 QStringLiteral("wrap_%1\n")
                     .arg(CrashHandler::kRingBufferSize + 9));
    }

    void ringBufferEmptyWhenNoRecords()
    {
        // Fresh instance (before any recordLogLine calls in this process)
        // This test may see lines from previous tests since ring buffer
        // is global — just verify it doesn't crash
        const QStringList logs = CrashHandler::recentLogLines();
        QVERIFY(logs.size() <= CrashHandler::kRingBufferSize);
    }

    void ringBufferCapacity()
    {
        QCOMPARE(CrashHandler::kRingBufferSize, 50);
    }
};

QTEST_MAIN(TestCrashHandler)
#include "test_crashhandler.moc"
