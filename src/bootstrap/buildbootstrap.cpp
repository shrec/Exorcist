#include "buildbootstrap.h"

#include "../build/toolchainmanager.h"
#include "../build/cmakeintegration.h"
#include "../build/buildtoolbar.h"
#include "../build/outputpanel.h"

#include <QFileInfo>

BuildBootstrap::BuildBootstrap(QObject *parent)
    : QObject(parent)
{
}

void BuildBootstrap::initialize()
{
    m_toolchainMgr     = new ToolchainManager(this);
    m_cmakeIntegration = new CMakeIntegration(this);
    m_cmakeIntegration->setToolchainManager(m_toolchainMgr);

    m_outputPanel  = new OutputPanel(nullptr);
    m_buildToolbar = new BuildToolbar(nullptr);
}

void BuildBootstrap::wireConnections()
{
    m_buildToolbar->setCMakeIntegration(m_cmakeIntegration);
    m_buildToolbar->setToolchainManager(m_toolchainMgr);

    // Configure → CMake configure
    connect(m_buildToolbar, &BuildToolbar::configureRequested, this, [this]() {
        m_outputPanel->clear();
        m_cmakeIntegration->configure();
        emit showOutputDock();
    });

    // After configure succeeds, restart clangd and refresh targets
    connect(m_cmakeIntegration, &CMakeIntegration::configureFinished,
            this, [this](bool success, const QString &) {
        if (!success) return;
        const QString ccjson = m_cmakeIntegration->compileCommandsPath();
        if (!ccjson.isEmpty())
            emit restartClangd(QFileInfo(ccjson).absolutePath());
        m_buildToolbar->refresh();
    });

    // Build → CMake build
    connect(m_buildToolbar, &BuildToolbar::buildRequested, this, [this]() {
        m_outputPanel->clear();
        m_cmakeIntegration->build();
        emit showOutputDock();
    });

    // Stop (build cancel)
    connect(m_buildToolbar, &BuildToolbar::stopRequested, this, [this]() {
        if (m_cmakeIntegration->isBuilding())
            m_cmakeIntegration->cancelBuild();
    });

    // Clean
    connect(m_buildToolbar, &BuildToolbar::cleanRequested, this, [this]() {
        m_cmakeIntegration->clean();
    });

    // Forward CMake build output to Output panel
    connect(m_cmakeIntegration, &CMakeIntegration::buildOutput,
            this, [this](const QString &line, bool isError) {
        m_outputPanel->appendBuildLine(line, isError);
    });

    // Push build diagnostics after CMake build finishes
    connect(m_cmakeIntegration, &CMakeIntegration::buildFinished,
            this, [this](bool, int) {
        emit buildDiagnosticsReady();
    });
}
