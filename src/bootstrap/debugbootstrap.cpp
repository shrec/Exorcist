#include "debugbootstrap.h"

#include "../debug/gdbmiadapter.h"
#include "../debug/debugpanel.h"

DebugBootstrap::DebugBootstrap(QObject *parent)
    : QObject(parent)
{
}

void DebugBootstrap::initialize()
{
    m_debugAdapter = new GdbMiAdapter(this);
    m_debugPanel   = new DebugPanel(nullptr);
    m_debugPanel->setAdapter(m_debugAdapter);
}

void DebugBootstrap::wireConnections()
{
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
