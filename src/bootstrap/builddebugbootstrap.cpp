#include "builddebugbootstrap.h"

BuildDebugBootstrap::BuildDebugBootstrap(QObject *parent)
    : QObject(parent)
    , m_build(this)
    , m_debug(this)
{
}

void BuildDebugBootstrap::initialize()
{
    m_build.initialize();
    m_debug.initialize();
}

void BuildDebugBootstrap::wireConnections()
{
    m_build.wireConnections();
    m_debug.wireConnections(m_build.cmakeIntegration(),
                            m_build.toolchainManager(),
                            m_build.buildToolbar());

    // Forward build signals
    connect(&m_build, &BuildBootstrap::showOutputDock,
            this,     &BuildDebugBootstrap::showOutputDock);
    connect(&m_build, &BuildBootstrap::statusMessage,
            this,     &BuildDebugBootstrap::statusMessage);
    connect(&m_build, &BuildBootstrap::restartClangd,
            this,     &BuildDebugBootstrap::restartClangd);
    connect(&m_build, &BuildBootstrap::buildDiagnosticsReady,
            this,     &BuildDebugBootstrap::buildDiagnosticsReady);

    // Forward debug signals
    connect(&m_debug, &DebugBootstrap::navigateToSource,
            this,     &BuildDebugBootstrap::navigateToSource);
    connect(&m_debug, &DebugBootstrap::showRunDock,
            this,     &BuildDebugBootstrap::showRunDock);
    connect(&m_debug, &DebugBootstrap::showDebugDock,
            this,     &BuildDebugBootstrap::showDebugDock);
    connect(&m_debug, &DebugBootstrap::statusMessage,
            this,     &BuildDebugBootstrap::statusMessage);
    connect(&m_debug, &DebugBootstrap::debugStopped,
            this,     &BuildDebugBootstrap::debugStopped);
    connect(&m_debug, &DebugBootstrap::debugTerminated,
            this,     &BuildDebugBootstrap::debugTerminated);
}

void BuildDebugBootstrap::setWorkingDir(const QString &dir)
{
    m_build.setWorkingDir(dir);
    m_debug.setWorkingDir(dir);
}
