#include "debugplugin.h"

#include "gdbmiadapter.h"
#include "debug/debugpanel.h"
#include "ui/themeicons.h"

#include "sdk/idebugadapter.h"
#include "sdk/idebugservice.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"

#include <QAction>
#include <QHash>

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

        // Adapter stopped → request stack trace (use actual thread ID, not 0)
        connect(m_adapter, &IDebugAdapter::stopped,
                this, [this](int threadId, DebugStopReason, const QString &) {
            m_adapter->requestStackTrace(threadId);
        });

        // Stack trace received → debugStopped
        connect(m_adapter, &IDebugAdapter::stackTraceReceived,
                this, [this](int, const QList<DebugFrame> &frames) {
            emit debugStopped(frames);
        });

        // Adapter terminated → debugTerminated; clear breakpoint ID map
        connect(m_adapter, &IDebugAdapter::terminated,
                this, [this]() {
            m_bpIdMap.clear();
            emit debugTerminated();
        });

        // Track confirmed breakpoint IDs so we can remove them by ID
        connect(m_adapter, &IDebugAdapter::breakpointSet,
                this, [this](const DebugBreakpoint &bp) {
            if (bp.id >= 0)
                m_bpIdMap.insert(locationKey(bp.filePath, bp.line), bp.id);
        });
        connect(m_adapter, &IDebugAdapter::breakpointRemoved,
                this, [this](int bpId) {
            for (auto it = m_bpIdMap.begin(); it != m_bpIdMap.end(); ) {
                if (it.value() == bpId)
                    it = m_bpIdMap.erase(it);
                else
                    ++it;
            }
        });
    }

    void addBreakpointEntry(const QString &filePath, int line) override
    {
        m_panel->addBreakpointEntry(filePath, line);
    }

    void removeBreakpointEntry(const QString &filePath, int line) override
    {
        m_panel->removeBreakpointEntry(filePath, line);
        m_bpIdMap.remove(locationKey(filePath, line));
    }

    int breakpointIdForLocation(const QString &filePath, int line) const override
    {
        return m_bpIdMap.value(locationKey(filePath, line), -1);
    }

private:
    static QString locationKey(const QString &fp, int line)
    {
        return fp + QLatin1Char(':') + QString::number(line);
    }

    GdbMiAdapter        *m_adapter;
    DebugPanel          *m_panel;
    QHash<QString, int>  m_bpIdMap;  // "file:line" → adapter breakpoint ID
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

    // Auto-show debug dock when debugger starts or stops
    connect(m_adapter, &IDebugAdapter::started, this, [this]() {
        showPanel(QStringLiteral("DebugDock"));
    });
    connect(m_debugService, &IDebugService::debugStopped,
            this, [this](const QList<DebugFrame> &) {
        showPanel(QStringLiteral("DebugDock"));
    });
    // Show adapter errors to the user
    connect(m_adapter, &IDebugAdapter::error, this, [this](const QString &msg) {
        showInfo(tr("Debugger error: %1").arg(msg));
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
    if (viewId == QLatin1String("DebugDock") && m_panel) {
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

    // Hide debug toolbar until a workspace is opened
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("debug"), false);

    // Add toolbar actions individually so we can attach SVG icons.
    static const struct {
        const char *text;
        const char *commandId;
        const char *iconName;
        const char *iconColor;   // nullptr → default theme color
        Qt::Key     key;
        Qt::KeyboardModifiers mods;
        bool separatorBefore;
    } kActions[] = {
        { QT_TR_NOOP("Launch"),    "debug.launch",    "play",      "#4cac47", Qt::Key_F5,  Qt::NoModifier,    false },
        { QT_TR_NOOP("Continue"),  "debug.continue",  "continue",  nullptr,   Qt::Key_unknown, Qt::NoModifier, false },
        { QT_TR_NOOP("Step Over"), "debug.stepOver",  "step-over", nullptr,   Qt::Key_F10, Qt::NoModifier,    true  },
        { QT_TR_NOOP("Step Into"), "debug.stepInto",  "step-into", nullptr,   Qt::Key_F11, Qt::NoModifier,    false },
        { QT_TR_NOOP("Step Out"),  "debug.stepOut",   "step-out",  nullptr,   Qt::Key_F11, Qt::ShiftModifier, false },
        { QT_TR_NOOP("Stop"),      "debug.terminate", "terminate", "#c72e24", Qt::Key_F5,  Qt::ShiftModifier, true  },
    };

    for (const auto &a : kActions) {
        if (a.separatorBefore)
            addToolBarSeparator(QStringLiteral("debug"));

        QKeySequence ks;
        if (a.key != Qt::Key_unknown)
            ks = QKeySequence(a.mods | a.key);

        auto *act = addToolBarCommand(QStringLiteral("debug"),
                                     tr(a.text),
                                     QString::fromLatin1(a.commandId),
                                     this, ks);
        if (act) {
            const QString color = a.iconColor
                ? QString::fromLatin1(a.iconColor)
                : ThemeIcons::defaultColor();
            act->setIcon(ThemeIcons::icon(QString::fromLatin1(a.iconName), color));
        }
    }

    addMenuCommands(IMenuManager::Debug, {
        {tr("&Launch Debugger"), QStringLiteral("debug.launch"), QKeySequence(Qt::Key_F5), true},
        {tr("&Continue"), QStringLiteral("debug.continue")},
        {tr("Step &Over"), QStringLiteral("debug.stepOver"), QKeySequence(Qt::Key_F10)},
        {tr("Step &Into"), QStringLiteral("debug.stepInto"), QKeySequence(Qt::Key_F11)},
        {tr("Step O&ut"), QStringLiteral("debug.stepOut"), QKeySequence(Qt::SHIFT | Qt::Key_F11)},
        {tr("&Terminate Debug Session"), QStringLiteral("debug.terminate"), QKeySequence(Qt::SHIFT | Qt::Key_F5), true},
    }, this);
}

void DebugPlugin::onWorkspaceChanged(const QString &root)
{
    Q_UNUSED(root)
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("debug"), true);
}

#include "debugplugin.moc"
