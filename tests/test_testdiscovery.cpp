#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>

#include "testing/testdiscoveryservice.h"

class TestTestDiscovery : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_tmpDir = QDir::tempPath() + QStringLiteral("/exorcist_test_discovery_XXXXXX");
        QDir().mkpath(m_tmpDir);
    }

    void cleanupTestCase()
    {
        QDir(m_tmpDir).removeRecursively();
    }

    // ── Default state ─────────────────────────────────────────────────────

    void defaultState()
    {
        TestDiscoveryService svc;
        QVERIFY(svc.buildDirectory().isEmpty());
        QVERIFY(svc.tests().isEmpty());
        QVERIFY(!svc.isBusy());
    }

    void setBuildDirectory()
    {
        TestDiscoveryService svc;
        svc.setBuildDirectory(QStringLiteral("/some/path"));
        QCOMPARE(svc.buildDirectory(), QStringLiteral("/some/path"));
    }

    void setBuildDirectory_clearsTests()
    {
        TestDiscoveryService svc;
        svc.setBuildDirectory(QStringLiteral("/first"));
        svc.setBuildDirectory(QStringLiteral("/second"));
        QVERIFY(svc.tests().isEmpty());
    }

    // ── Discovery with no CTest files ─────────────────────────────────────

    void discover_noBuildDir()
    {
        TestDiscoveryService svc;
        QSignalSpy spy(&svc, &TestDiscoveryService::discoveryFinished);
        svc.discoverTests(); // no build dir set
        QCOMPARE(spy.count(), 0);
        QVERIFY(svc.tests().isEmpty());
    }

    void discover_noCTestFile()
    {
        TestDiscoveryService svc;
        svc.setBuildDirectory(m_tmpDir); // no CTestTestfile.cmake
        QSignalSpy spy(&svc, &TestDiscoveryService::discoveryFinished);
        svc.discoverTests();
        QCOMPARE(spy.count(), 0); // silently returns
    }

    // ── JSON parsing ──────────────────────────────────────────────────────

    void parseJson_empty()
    {
        TestDiscoveryService svc;
        // Use reflection to call private method? No, use the real path instead.
        // Create a fake CTestTestfile.cmake so discovery runs
        createCTestFile();
        svc.setBuildDirectory(m_tmpDir);

        // Since ctest won't find real tests in m_tmpDir, just check no crash
        svc.discoverTests();
        // May or may not have tests depending on ctest version, but shouldn't crash
    }

    // ── TestItem struct ───────────────────────────────────────────────────

    void testItem_defaults()
    {
        TestItem item;
        QVERIFY(item.name.isEmpty());
        QCOMPARE(item.index, -1);
        QCOMPARE(item.status, TestItem::Unknown);
        QVERIFY(item.output.isEmpty());
        QCOMPARE(item.duration, 0.0);
    }

    void testItem_statusValues()
    {
        QCOMPARE(static_cast<int>(TestItem::Unknown),  0);
        QCOMPARE(static_cast<int>(TestItem::Running),  1);
        QCOMPARE(static_cast<int>(TestItem::Passed),   2);
        QCOMPARE(static_cast<int>(TestItem::Failed),   3);
        QCOMPARE(static_cast<int>(TestItem::Skipped),  4);
    }

    // ── Run with no tests ─────────────────────────────────────────────────

    void runAll_noTests()
    {
        TestDiscoveryService svc;
        QSignalSpy spy(&svc, &TestDiscoveryService::allTestsFinished);
        svc.runAllTests(); // empty test list
        QCOMPARE(spy.count(), 0); // nothing to run
    }

    void runTest_invalidIndex()
    {
        TestDiscoveryService svc;
        svc.runTest(-1); // should not crash
        svc.runTest(999);
    }

    // ── Integration test with real build (if available) ───────────────────

    void discover_realBuild()
    {
        const QString buildDir = QStringLiteral("D:/Dev/Exorcist/build-llvm");
        if (!QFileInfo::exists(buildDir + QStringLiteral("/CTestTestfile.cmake"))) {
            QSKIP("Real build directory not found, skipping integration test");
        }

        TestDiscoveryService svc;
        svc.setBuildDirectory(buildDir);

        QSignalSpy spy(&svc, &TestDiscoveryService::discoveryFinished);
        svc.discoverTests();

        QCOMPARE(spy.count(), 1);
        QVERIFY(svc.tests().size() >= 15); // We know we have 15+ tests

        // Check first test has valid data
        const auto &first = svc.tests().first();
        QVERIFY(!first.name.isEmpty());
        QVERIFY(first.index > 0);
        QVERIFY(!first.command.isEmpty());
    }

    void runSingle_realBuild()
    {
        const QString buildDir = QStringLiteral("D:/Dev/Exorcist/build-llvm");
        if (!QFileInfo::exists(buildDir + QStringLiteral("/CTestTestfile.cmake"))) {
            QSKIP("Real build directory not found, skipping integration test");
        }

        TestDiscoveryService svc;
        svc.setBuildDirectory(buildDir);
        svc.discoverTests();

        if (svc.tests().isEmpty())
            QSKIP("No tests discovered");

        // Find ServiceRegistry test (should be fast)
        int srIdx = -1;
        for (int i = 0; i < svc.tests().size(); ++i) {
            if (svc.tests()[i].name == QStringLiteral("ServiceRegistry")) {
                srIdx = i;
                break;
            }
        }
        if (srIdx < 0)
            QSKIP("ServiceRegistry test not found");

        QSignalSpy startSpy(&svc, &TestDiscoveryService::testStarted);
        QSignalSpy finishSpy(&svc, &TestDiscoveryService::testFinished);
        QSignalSpy allSpy(&svc, &TestDiscoveryService::allTestsFinished);

        svc.runTest(srIdx);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(finishSpy.count(), 1);
        QCOMPARE(allSpy.count(), 1);
        QCOMPARE(svc.tests()[srIdx].status, TestItem::Passed);
        QVERIFY(!svc.tests()[srIdx].output.isEmpty());
    }

private:
    void createCTestFile()
    {
        QFile f(m_tmpDir + QStringLiteral("/CTestTestfile.cmake"));
        if (f.open(QIODevice::WriteOnly))
            f.write("# empty\n");
    }

    QString m_tmpDir;
};

QTEST_MAIN(TestTestDiscovery)
#include "test_testdiscovery.moc"
