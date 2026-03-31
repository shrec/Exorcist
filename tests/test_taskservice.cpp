#include <QTest>

#include "sdk/ibuildsystem.h"
#include "sdk/taskserviceimpl.h"
#include "serviceregistry.h"

class StubBuildSystem : public IBuildSystem
{
    Q_OBJECT

public:
    using IBuildSystem::IBuildSystem;

    void setProjectRoot(const QString &) override {}
    bool hasProject() const override { return true; }
    void configure() override { ++configureCalls; }
    void build(const QString &target = {}) override { ++buildCalls; lastTarget = target; }
    void clean() override { ++cleanCalls; }
    void cancelBuild() override { ++cancelCalls; }
    bool isBuilding() const override { return building; }
    QStringList targets() const override { return {}; }
    QString buildDirectory() const override { return {}; }
    QString compileCommandsPath() const override { return {}; }

    int configureCalls = 0;
    int buildCalls = 0;
    int cleanCalls = 0;
    int cancelCalls = 0;
    bool building = false;
    QString lastTarget;
};

class TestTaskService : public QObject
{
    Q_OBJECT

private slots:
    void configureTaskDelegatesToBuildSystem();
    void buildTaskDelegatesToBuildSystem();
    void targetedBuildDelegatesToBuildSystem();
    void cleanTaskDelegatesToBuildSystem();
    void cancelBuildTaskDelegatesToBuildSystem();
    void runningStateReflectsBuildSystem();
};

void TestTaskService::configureTaskDelegatesToBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);

    tasks.runTask(QStringLiteral("build.configure"));

    QCOMPARE(buildSystem.configureCalls, 1);
}

void TestTaskService::buildTaskDelegatesToBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);

    tasks.runTask(QStringLiteral("build.build"));

    QCOMPARE(buildSystem.buildCalls, 1);
    QVERIFY(buildSystem.lastTarget.isEmpty());
}

void TestTaskService::targetedBuildDelegatesToBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);

    tasks.runTask(QStringLiteral("build.build:all"));

    QCOMPARE(buildSystem.buildCalls, 1);
    QCOMPARE(buildSystem.lastTarget, QStringLiteral("all"));
}

void TestTaskService::cleanTaskDelegatesToBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);

    tasks.runTask(QStringLiteral("build.clean"));

    QCOMPARE(buildSystem.cleanCalls, 1);
}

void TestTaskService::cancelBuildTaskDelegatesToBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);

    tasks.cancelTask(QStringLiteral("build.build"));

    QCOMPARE(buildSystem.cancelCalls, 1);
}

void TestTaskService::runningStateReflectsBuildSystem()
{
    ServiceRegistry services;
    StubBuildSystem buildSystem;
    TaskServiceImpl tasks;
    services.registerService(QStringLiteral("buildSystem"), &buildSystem);
    tasks.setServiceRegistry(&services);
    buildSystem.building = true;

    QVERIFY(tasks.isTaskRunning(QStringLiteral("build.build")));
    QVERIFY(!tasks.isTaskRunning(QStringLiteral("testing.runAll")));
}

QTEST_MAIN(TestTaskService)
#include "test_taskservice.moc"