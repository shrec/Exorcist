#include "debugbootstrap.h"

#include "../build/debuglaunchcontroller.h"
#include "../build/buildtoolbar.h"
#include "../build/cmakeintegration.h"
#include "../build/toolchainmanager.h"
#include "../debug/gdbmiadapter.h"
#include "../debug/debugpanel.h"

DebugBootstrap::DebugBootstrap(QObject *parent)
    : QObject(parent)
{
}

void DebugBootstrap::initialize()
{
    m_debugLauncher = new DebugLaunchController(this);
    m_debugAdapter  = new GdbMiAdapter(this);
    m_debugPanel    = new DebugPanel(nullptr);
    m_debugPanel->setAdapter(m_debugAdapter);
}

void DebugBootstrap::wireConnections(CMakeIntegration *cmake,
                                     ToolchainManager *toolchain,
                                     BuildToolbar *toolbar)
{
    m_debugLauncher->setCMakeIntegration(cmake);
    m_debugLauncher->setToolchainManager(toolchain);
    m_debugLauncher->setDebugAdapter(m_debugAdapter);
    toolbar->setDebugAdapter(m_debugAdapter);

    // Run without debugging (Ctrl+F5)
    connect(toolbar, &BuildToolbar::runRequested,
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
    connect(toolbar, &BuildToolbar::debugRequested,
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

    // Stop (debug stop — build stop is handled by BuildBootstrap)
    connect(toolbar, &BuildToolbar::stopRequested, this, [this]() {
        m_debugLauncher->stopDebugging();
    });

    // Forward launch errors
    connect(m_debugLauncher, &DebugLaunchController::launchError,
            this, [this](const QString &msg) {
        emit statusMessage(tr("Launch error: %1").arg(msg), 5000);
    });

    // Debug panel → navigate to source
    connect(m_debugPanel, &DebugPanel::navigateToSource,
            this, &DebugBootstrap::navigateToSource);

    // Debug stopped → request stack trace
    connect(m_debugAdapter, &GdbMiAdapter::stopped,
            this, [this](int, DebugStopReason, const QString &) {
        m_debugAdapter->requestStackTrace(0);
    });

    // Stack trace received → signal for editor highlighting
    connect(m_debugAdapter, &GdbMiAdapter::stackTraceReceived,
            this, [this](int, const QList<DebugFrame> &frames) {
        emit debugStopped(frames);
    });

    // Debug session ended
    connect(m_debugAdapter, &GdbMiAdapter::terminated,
            this, [this]() {
        emit debugTerminated();
    });
}
