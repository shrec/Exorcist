#include "taskserviceimpl.h"

#include "serviceregistry.h"
#include "sdk/ibuildsystem.h"

namespace {

IBuildSystem *buildSystemFor(ServiceRegistry *registry)
{
    if (!registry)
        return nullptr;
    return registry->service<IBuildSystem>(QStringLiteral("buildSystem"));
}

bool isBuildTask(const QString &taskId)
{
    return taskId == QLatin1String("build.configure")
        || taskId == QLatin1String("build.build")
        || taskId.startsWith(QLatin1String("build.build:"))
        || taskId == QLatin1String("build.clean");
}

}

TaskServiceImpl::TaskServiceImpl(QObject *parent)
    : QObject(parent)
{
}

void TaskServiceImpl::setServiceRegistry(ServiceRegistry *registry)
{
    m_registry = registry;
}

void TaskServiceImpl::runTask(const QString &taskId)
{
    auto *buildSystem = buildSystemFor(m_registry);
    if (!buildSystem)
        return;

    if (taskId == QLatin1String("build.configure")) {
        buildSystem->configure();
        return;
    }

    if (taskId == QLatin1String("build.build")) {
        buildSystem->build();
        return;
    }

    if (taskId.startsWith(QLatin1String("build.build:"))) {
        buildSystem->build(taskId.mid(QStringLiteral("build.build:").size()));
        return;
    }

    if (taskId == QLatin1String("build.clean"))
        buildSystem->clean();
}

void TaskServiceImpl::cancelTask(const QString &taskId)
{
    auto *buildSystem = buildSystemFor(m_registry);
    if (!buildSystem || !isBuildTask(taskId))
        return;

    buildSystem->cancelBuild();
}

bool TaskServiceImpl::isTaskRunning(const QString &taskId) const
{
    auto *buildSystem = buildSystemFor(m_registry);
    if (!buildSystem || !isBuildTask(taskId))
        return false;

    return buildSystem->isBuilding();
}