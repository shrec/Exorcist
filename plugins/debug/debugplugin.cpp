#include "debugplugin.h"

#include "gdbmiadapter.h"
#include "debug/debugpanel.h"

#include "sdk/idebugadapter.h"
#include "sdk/idebugservice.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"

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

bool DebugPlugin::initializePlugin()
{
    // Create core debug objects — bare new required: GdbMiAdapter is
    // QObject with parent ownership, DebugPanel is a top-level QWidget
    // reparented later via createView().
    m_adapter = new GdbMiAdapter(this);
    m_panel   = new DebugPanel(nullptr);
    m_panel->setAdapter(m_adapter);

    // Create service bridge (replaces DebugBootstrap's signal wiring)
    m_debugService = new DebugServiceBridge(m_adapter, m_panel, this);

    // Register services
    registerService(QStringLiteral("debugAdapter"), m_adapter);
    registerService(QStringLiteral("debugService"), m_debugService);

    // Auto-show debug dock when debugger stops (breakpoint hit, step)
    connect(m_debugService, &IDebugService::debugStopped,
            this, [this](const QList<DebugFrame> &) {
        showPanel(QStringLiteral("DebugDock"));
    });

    registerCommands();
    installMenusAndToolBar();

    return true;
}

void DebugPlugin::shutdownPlugin()
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

void DebugPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("debug.launch"), tr("Launch Debugger"), [this, cmds]() {
        if (cmds->hasCommand(QStringLiteral("build.debug"))) {
            cmds->executeCommand(QStringLiteral("build.debug"));
            return;
        }
        if (m_adapter && m_adapter->isRunning())
            m_adapter->continueExecution();
    });

    cmds->registerCommand(QStringLiteral("debug.continue"), tr("Continue Execution"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->continueExecution();
    });

    cmds->registerCommand(QStringLiteral("debug.stepOver"), tr("Step Over"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->stepOver();
    });

    cmds->registerCommand(QStringLiteral("debug.stepInto"), tr("Step Into"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->stepInto();
    });

    cmds->registerCommand(QStringLiteral("debug.stepOut"), tr("Step Out"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->stepOut();
    });

    cmds->registerCommand(QStringLiteral("debug.terminate"), tr("Terminate Debug Session"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->terminate();
    });
}

void DebugPlugin::installMenusAndToolBar()
{
    createToolBar(QStringLiteral("debug"), tr("Debug"));
    addToolBarCommands(QStringLiteral("debug"), {
        {tr("Launch"), QStringLiteral("debug.launch"), QKeySequence(Qt::Key_F5)},
        {tr("Continue"), QStringLiteral("debug.continue")},
        {tr("Step Over"), QStringLiteral("debug.stepOver"), QKeySequence(Qt::Key_F10), true},
        {tr("Step Into"), QStringLiteral("debug.stepInto"), QKeySequence(Qt::Key_F11)},
        {tr("Step Out"), QStringLiteral("debug.stepOut"), QKeySequence(Qt::SHIFT | Qt::Key_F11)},
        {tr("Stop"), QStringLiteral("debug.terminate"), QKeySequence(Qt::SHIFT | Qt::Key_F5), true},
    }, this);

    addMenuCommands(IMenuManager::Debug, {
        {tr("&Launch Debugger"), QStringLiteral("debug.launch"), QKeySequence(Qt::Key_F5), true},
        {tr("&Continue"), QStringLiteral("debug.continue")},
        {tr("Step &Over"), QStringLiteral("debug.stepOver"), QKeySequence(Qt::Key_F10)},
        {tr("Step &Into"), QStringLiteral("debug.stepInto"), QKeySequence(Qt::Key_F11)},
        {tr("Step O&ut"), QStringLiteral("debug.stepOut"), QKeySequence(Qt::SHIFT | Qt::Key_F11)},
        {tr("&Terminate Debug Session"), QStringLiteral("debug.terminate"), QKeySequence(Qt::SHIFT | Qt::Key_F5), true},
    }, this);
}

#include "debugplugin.moc"
