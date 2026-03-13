#include <QtTest>
#include <QSignalSpy>

#include "build/buildsystemservice.h"
#include "build/cmakeintegration.h"
#include "testing/testrunnerservice.h"
#include "testing/testdiscoveryservice.h"
#include "sdk/ibuildsystem.h"
#include "sdk/itestrunner.h"

// ── TestBuildTestServices ────────────────────────────────────────────────────

class TestBuildTestServices : public QObject
{
    Q_OBJECT

private slots:
    void buildSystemService_hasProject();
    void buildSystemService_buildDirectory();
    void buildSystemService_targets();
    void buildSystemService_forwardsBuildSignals();
    void buildSystemService_forwardsBuildFinished();
    void buildSystemService_isBuilding();

    void testRunnerService_hasTests_empty();
    void testRunnerService_tests_returnsDiscoveryTests();
    void testRunnerService_forwardsDiscoveryFinished();
    void testRunnerService_forwardsTestStarted();
    void testRunnerService_forwardsTestFinished();
    void testRunnerService_forwardsAllTestsFinished();
};

// ── BuildSystemService tests ─────────────────────────────────────────────────

void TestBuildTestServices::buildSystemService_hasProject()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    // No project root set → no project
    QVERIFY(!svc.hasProject());
}

void TestBuildTestServices::buildSystemService_buildDirectory()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    // Default build directory is empty
    QVERIFY(svc.buildDirectory().isEmpty());
}

void TestBuildTestServices::buildSystemService_targets()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    // No project → empty targets
    QVERIFY(svc.targets().isEmpty());
}

void TestBuildTestServices::buildSystemService_forwardsBuildSignals()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    QSignalSpy spy(&svc, &IBuildSystem::buildOutput);

    // Emit from cmake, verify forwarded through service
    emit cmake.buildOutput(QStringLiteral("Building..."), false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Building..."));
    QCOMPARE(spy.at(0).at(1).toBool(), false);
}

void TestBuildTestServices::buildSystemService_forwardsBuildFinished()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    QSignalSpy spy(&svc, &IBuildSystem::buildFinished);

    emit cmake.buildFinished(true, 0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
}

void TestBuildTestServices::buildSystemService_isBuilding()
{
    CMakeIntegration cmake;
    BuildSystemService svc(&cmake);

    // Not building by default
    QVERIFY(!svc.isBuilding());
}

// ── TestRunnerService tests ──────────────────────────────────────────────────

void TestBuildTestServices::testRunnerService_hasTests_empty()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    QVERIFY(!svc.hasTests());
}

void TestBuildTestServices::testRunnerService_tests_returnsDiscoveryTests()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    // Discovery not run → empty list
    QVERIFY(svc.tests().isEmpty());
}

void TestBuildTestServices::testRunnerService_forwardsDiscoveryFinished()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    QSignalSpy spy(&svc, &ITestRunner::discoveryFinished);

    emit discovery.discoveryFinished();

    QCOMPARE(spy.count(), 1);
}

void TestBuildTestServices::testRunnerService_forwardsTestStarted()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    QSignalSpy spy(&svc, &ITestRunner::testStarted);

    emit discovery.testStarted(3);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 3);
}

void TestBuildTestServices::testRunnerService_forwardsTestFinished()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    QSignalSpy spy(&svc, &ITestRunner::testFinished);

    emit discovery.testFinished(5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 5);
}

void TestBuildTestServices::testRunnerService_forwardsAllTestsFinished()
{
    TestDiscoveryService discovery;
    TestRunnerService svc(&discovery);

    QSignalSpy spy(&svc, &ITestRunner::allTestsFinished);

    emit discovery.allTestsFinished();

    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestBuildTestServices)
#include "test_buildtestservices.moc"
