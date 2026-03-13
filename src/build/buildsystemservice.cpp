#include "buildsystemservice.h"
#include "cmakeintegration.h"

BuildSystemService::BuildSystemService(CMakeIntegration *cmake, QObject *parent)
    : IBuildSystem(parent)
    , m_cmake(cmake)
{
    connect(m_cmake, &CMakeIntegration::buildOutput,
            this,    &IBuildSystem::buildOutput);
    connect(m_cmake, &CMakeIntegration::configureFinished,
            this,    &IBuildSystem::configureFinished);
    connect(m_cmake, &CMakeIntegration::buildFinished,
            this,    &IBuildSystem::buildFinished);
    connect(m_cmake, &CMakeIntegration::cleanFinished,
            this,    &IBuildSystem::cleanFinished);
}

bool BuildSystemService::hasProject() const
{
    return m_cmake->hasCMakeProject();
}

void BuildSystemService::configure()
{
    m_cmake->configure();
}

void BuildSystemService::build(const QString &target)
{
    m_cmake->build(target);
}

void BuildSystemService::clean()
{
    m_cmake->clean();
}

void BuildSystemService::cancelBuild()
{
    m_cmake->cancelBuild();
}

bool BuildSystemService::isBuilding() const
{
    return m_cmake->isBuilding();
}

QStringList BuildSystemService::targets() const
{
    return m_cmake->discoverTargets();
}

QString BuildSystemService::buildDirectory() const
{
    return m_cmake->buildDirectory();
}

QString BuildSystemService::compileCommandsPath() const
{
    return m_cmake->compileCommandsPath();
}
