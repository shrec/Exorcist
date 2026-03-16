#include "debugplugin.h"

#include "gdbmiadapter.h"
#include "debugpanel.h"

#include "sdk/idebugadapter.h"
#include "sdk/idebugservice.h"
#include "sdk/ihostservices.h"
#include "core/idockmanager.h"

// ── DebugServiceBridge ───────────────────────────────────────────────────────
// Concrete IDebugService that bridges adapter/panel signals to the core IDE.

class DebugServiceBridge : public IDebugService
{
    Q_OBJECT

public:
    explicit DebugServiceBridge(GdbMiAdapter *adapter, DebugPanel *panel,
                                QObject *parent = nullptr)
        : IDebugService(parent), m_adapter(adapter), m_panel(panel)
    {
        // Panel → navigate to source
        connect(m_panel, &DebugPanel::navigateToSource,
                this, &IDebugService::navigateToSource);

        // Adapter stopped → request stack trace
        connect(m_adapter, &IDebugAdapter::stopped,
                this, [this](int, DebugStopReason, const QString &) {
            m_adapter->requestStackTrace(0);
        });

        // Stack trace received → debugStopped
        connect(m_adapter, &IDebugAdapter::stackTraceReceived,
                this, [this](int, const QList<DebugFrame> &frames) {
            emit debugStopped(frames);
        });

        // Adapter terminated → debugTerminated
        connect(m_adapter, &IDebugAdapter::terminated,
                this, &IDebugService::debugTerminated);
    }

    void addBreakpointEntry(const QString &filePath, int line) override
    {
        m_panel->addBreakpointEntry(filePath, line);
    }

    void removeBreakpointEntry(const QString &filePath, int line) override
    {
        m_panel->removeBreakpointEntry(filePath, line);
    }

private:
    GdbMiAdapter *m_adapter;
    DebugPanel   *m_panel;
};

// ── DebugPlugin ──────────────────────────────────────────────────────────────

DebugPlugin::DebugPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo DebugPlugin::info() const
{
    return { QStringLiteral("org.exorcist.debug"),
             QStringLiteral("Debug"),
             QStringLiteral("1.0.0"),
             QStringLiteral("GDB/MI debug adapter and debug panel") };
}

bool DebugPlugin::initialize(IHostServices *host)
{
    m_host = host;

    // Create core debug objects — bare new required: GdbMiAdapter is
    // QObject with parent ownership, DebugPanel is a top-level QWidget
    // reparented later via createView().
    m_adapter = new GdbMiAdapter(this);
    m_panel   = new DebugPanel(nullptr);
    m_panel->setAdapter(m_adapter);

    // Create service bridge (replaces DebugBootstrap's signal wiring)
    m_debugService = new DebugServiceBridge(m_adapter, m_panel, this);

    // Register services
    m_host->registerService(QStringLiteral("debugAdapter"), m_adapter);
    m_host->registerService(QStringLiteral("debugService"), m_debugService);

    // Auto-show debug dock when debugger stops (breakpoint hit, step)
    if (auto *dockMgr = m_host->docks()) {
        connect(m_debugService, &IDebugService::debugStopped,
                this, [dockMgr](const QList<DebugFrame> &) {
            dockMgr->showPanel(QStringLiteral("DebugDock"));
        });
    }

    return true;
}

void DebugPlugin::shutdown()
{
}

QWidget *DebugPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("DebugDock")) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

#include "debugplugin.moc"
