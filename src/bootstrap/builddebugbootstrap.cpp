#include "builddebugbootstrap.h"

#include "../build/toolchainmanager.h"
#include "../build/cmakeintegration.h"
#include "../build/debuglaunchcontroller.h"
#include "../build/outputpanel.h"
#include "../build/buildtoolbar.h"
#include "../debug/gdbmiadapter.h"
#include "../debug/debugpanel.h"

#include <QFileInfo>

BuildDebugBootstrap::BuildDebugBootstrap(QObject *parent)
    : QObject(parent)
{
}

void BuildDebugBootstrap::initialize()
{
    m_toolchainMgr    = new ToolchainManager(this);
    m_cmakeIntegration = new CMakeIntegration(this);
    m_cmakeIntegration->setToolchainManager(m_toolchainMgr);

    m_debugLauncher = new DebugLaunchController(this);
    m_debugLauncher->setCMakeIntegration(m_cmakeIntegration);
    m_debugLauncher->setToolchainManager(m_toolchainMgr);

    m_debugAdapter = new GdbMiAdapter(this);
    m_debugPanel   = new DebugPanel(nullptr);
    m_debugPanel->setAdapter(m_debugAdapter);

    m_outputPanel = new OutputPanel(nullptr);

    m_buildToolbar = new BuildToolbar(nullptr);
}

void BuildDebugBootstrap::wireConnections()
{
    // Wire toolbar to subsystems
    m_debugLauncher->setDebugAdapter(m_debugAdapter);
    m_buildToolbar->setCMakeIntegration(m_cmakeIntegration);
    m_buildToolbar->setToolchainManager(m_toolchainMgr);
    m_buildToolbar->setDebugAdapter(m_debugAdapter);

    // Configure → CMake configure
    connect(m_buildToolbar, &BuildToolbar::configureRequested, this, [this]() {
        m_outputPanel->clear();
        m_cmakeIntegration->configure();
        emit showOutputDock();
    });

    // After configure succeeds, restart clangd and refresh toolbar targets
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

    // Run without debugging (Ctrl+F5)
    connect(m_buildToolbar, &BuildToolbar::runRequested,
            this, [this](const QString &exe) {
        if (exe.isEmpty()) {
            emit statusMessage(tr("No launch target selected"), 3000);
            return;
        }
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_debugLauncher->startWithoutDebugging(cfg);
        emit showRunDock();
    });

    // Debug (F5)
    connect(m_buildToolbar, &BuildToolbar::debugRequested,
            this, [this](const QString &exe) {
        if (exe.isEmpty()) {
            emit statusMessage(tr("No launch target selected"), 3000);
            return;
        }
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_debugLauncher->startDebugging(cfg);
        emit showDebugDock();
    });

    // Stop
    connect(m_buildToolbar, &BuildToolbar::stopRequested, this, [this]() {
        m_debugLauncher->stopDebugging();
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

    // Forward launch errors
    connect(m_debugLauncher, &DebugLaunchController::launchError,
            this, [this](const QString &msg) {
        emit statusMessage(tr("Launch error: %1").arg(msg), 5000);
    });

    // Debug panel → navigate to source on stack frame double-click
    connect(m_debugPanel, &DebugPanel::navigateToSource,
            this, &BuildDebugBootstrap::navigateToSource);

    // Debug stopped → request stack trace
    connect(m_debugAdapter, &GdbMiAdapter::stopped,
            this, [this](int /*threadId*/, DebugStopReason /*reason*/,
                         const QString &/*desc*/) {
        m_debugAdapter->requestStackTrace(0);
    });

    // Stack trace received → signal to MainWindow for editor highlighting
    connect(m_debugAdapter, &GdbMiAdapter::stackTraceReceived,
            this, [this](int /*threadId*/, const QList<DebugFrame> &frames) {
        emit debugStopped(frames);
    });

    // Debug session ended
    connect(m_debugAdapter, &GdbMiAdapter::terminated,
            this, [this]() {
        emit debugTerminated();
    });

}
