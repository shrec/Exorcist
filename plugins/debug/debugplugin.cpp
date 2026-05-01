#include "debugplugin.h"

#include "gdbmiadapter.h"
#include "memoryviewpanel.h"
#include "disassemblypanel.h"
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
        // Panel → navigate to source (signal-to-signal, cross-DLL safe)
        connect(m_panel, SIGNAL(navigateToSource(QString,int)),
                this, SIGNAL(navigateToSource(QString,int)));

        // Adapter stopped → request stack trace (use actual thread ID, not 0)
        // Use SIGNAL/SLOT for cross-DLL safety (IDebugAdapter MOC is in the exe).
        connect(m_adapter, SIGNAL(stopped(int,DebugStopReason,QString)),
                this, SLOT(onAdapterStopped(int,DebugStopReason,QString)));

        // Stack trace received → debugStopped
        connect(m_adapter, SIGNAL(stackTraceReceived(int,QList<DebugFrame>)),
                this, SLOT(onStackTraceReceived(int,QList<DebugFrame>)));

        // Adapter terminated → debugTerminated
        connect(m_adapter, SIGNAL(terminated()),
                this, SLOT(onAdapterTerminated()));

        // Track confirmed breakpoint IDs so we can remove them by ID
        connect(m_adapter, SIGNAL(breakpointSet(DebugBreakpoint)),
                this, SLOT(onBreakpointSet(DebugBreakpoint)));
        connect(m_adapter, SIGNAL(breakpointRemoved(int)),
                this, SLOT(onBreakpointRemoved(int)));
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

public slots:
    void onAdapterStopped(int threadId, DebugStopReason /*reason*/, const QString & /*desc*/)
    {
        m_adapter->requestStackTrace(threadId);
    }

    void onStackTraceReceived(int /*threadId*/, const QList<DebugFrame> &frames)
    {
        emit debugStopped(frames);
    }

    void onAdapterTerminated()
    {
        m_bpIdMap.clear();
        emit debugTerminated();
    }

    void onBreakpointSet(const DebugBreakpoint &bp)
    {
        if (bp.id >= 0)
            m_bpIdMap.insert(locationKey(bp.filePath, bp.line), bp.id);
    }

    void onBreakpointRemoved(int bpId)
    {
        for (auto it = m_bpIdMap.begin(); it != m_bpIdMap.end(); ) {
            if (it.value() == bpId)
                it = m_bpIdMap.erase(it);
            else
                ++it;
        }
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

// ── Cross-DLL adapter/service slots ──────────────────────────────────────────

void DebugPlugin::onAdapterStartedShowDock()
{
    showPanel(QStringLiteral("DebugDock"));
}

void DebugPlugin::onDebugStoppedShowDock()
{
    showPanel(QStringLiteral("DebugDock"));
}

void DebugPlugin::onAdapterErrorShowInfo(const QString &msg)
{
    showInfo(tr("Debugger error: %1").arg(msg));
}

// ── Plugin lifecycle ─────────────────────────────────────────────────────────

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

    // Memory view dock — top-level QWidget reparented later via createView().
    m_memoryView = new MemoryViewPanel(nullptr);
    m_memoryView->setAdapter(m_adapter);

    // Disassembly view dock — top-level QWidget reparented later via createView().
    m_disassemblyView = new DisassemblyPanel(nullptr);
    m_disassemblyView->setAdapter(m_adapter);

    // Create service bridge (replaces DebugBootstrap's signal wiring)
    m_debugService = new DebugServiceBridge(m_adapter, m_panel, this);

    // Register services
    registerService(QStringLiteral("debugAdapter"), m_adapter);
    registerService(QStringLiteral("debugService"), m_debugService);

    // Auto-show debug dock when debugger starts or stops.
    // Use SIGNAL/SLOT — PMF connect fails across DLL boundary.
    connect(m_adapter, SIGNAL(started()),
            this, SLOT(onAdapterStartedShowDock()));
    connect(m_debugService, SIGNAL(debugStopped(QList<DebugFrame>)),
            this, SLOT(onDebugStoppedShowDock()));
    connect(m_adapter, SIGNAL(error(QString)),
            this, SLOT(onAdapterErrorShowInfo(QString)));

    registerCommands();
    installMenusAndToolBar();

    return true;
}

void DebugPlugin::shutdownPlugin()
{
    // m_panel and m_memoryView are reparented into the dock when the view
    // is created via createView(), so the dock owns them. If a view was
    // never created the panel still belongs to the plugin — delete here.
    if (m_panel && !m_panel->parent()) {
        delete m_panel;
        m_panel = nullptr;
    }
    if (m_memoryView && !m_memoryView->parent()) {
        delete m_memoryView;
        m_memoryView = nullptr;
    }
    if (m_disassemblyView && !m_disassemblyView->parent()) {
        delete m_disassemblyView;
        m_disassemblyView = nullptr;
    }
}

QWidget *DebugPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("DebugDock") && m_panel) {
        m_panel->setParent(parent);
        return m_panel;
    }
    if (viewId == QLatin1String("MemoryDock") && m_memoryView) {
        m_memoryView->setParent(parent);
        return m_memoryView;
    }
    if (viewId == QLatin1String("DisassemblyDock") && m_disassemblyView) {
        m_disassemblyView->setParent(parent);
        return m_disassemblyView;
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

    auto debugCmd = [this](const char *name, void (IDebugAdapter::*fn)(int), int tid) {
        const bool hasAdapter = (m_adapter != nullptr);
        const bool isRunning  = hasAdapter && m_adapter->isRunning();
        if (m_adapter) {
            emit m_adapter->outputProduced(
                QStringLiteral("[CMD] debug.%1 — adapter=%2 isRunning=%3")
                    .arg(QString::fromLatin1(name))
                    .arg(hasAdapter).arg(isRunning),
                QStringLiteral("debug"));
        }
        if (hasAdapter && isRunning)
            (m_adapter->*fn)(tid);
    };

    cmds->registerCommand(QStringLiteral("debug.continue"), tr("Continue Execution"),
                          [debugCmd]() { debugCmd("continue",  &IDebugAdapter::continueExecution, 0); });
    cmds->registerCommand(QStringLiteral("debug.stepOver"), tr("Step Over"),
                          [debugCmd]() { debugCmd("stepOver",  &IDebugAdapter::stepOver, 0); });
    cmds->registerCommand(QStringLiteral("debug.stepInto"), tr("Step Into"),
                          [debugCmd]() { debugCmd("stepInto",  &IDebugAdapter::stepInto, 0); });
    cmds->registerCommand(QStringLiteral("debug.stepOut"), tr("Step Out"),
                          [debugCmd]() { debugCmd("stepOut",   &IDebugAdapter::stepOut, 0); });

    cmds->registerCommand(QStringLiteral("debug.terminate"), tr("Terminate Debug Session"), [this]() {
        if (m_adapter && m_adapter->isRunning())
            m_adapter->terminate();
    });

    cmds->registerCommand(QStringLiteral("debug.openMemoryView"), tr("Open Memory View"), [this]() {
        showPanel(QStringLiteral("MemoryDock"));
    });

    cmds->registerCommand(QStringLiteral("debug.openDisassembly"), tr("Open Disassembly"), [this]() {
        showPanel(QStringLiteral("DisassemblyDock"));
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
        // No shortcuts here — shortcuts are on the menu actions (addMenuCommands below).
        // Having the same shortcut on both toolbar + menu QActions causes Qt "ambiguous shortcut".
        { QT_TR_NOOP("Launch"),    "debug.launch",    "play",      "#4cac47", Qt::Key_unknown, Qt::NoModifier, false },
        { QT_TR_NOOP("Continue"),  "debug.continue",  "continue",  nullptr,   Qt::Key_unknown, Qt::NoModifier, false },
        { QT_TR_NOOP("Step Over"), "debug.stepOver",  "step-over", nullptr,   Qt::Key_unknown, Qt::NoModifier, true  },
        { QT_TR_NOOP("Step Into"), "debug.stepInto",  "step-into", nullptr,   Qt::Key_unknown, Qt::NoModifier, false },
        { QT_TR_NOOP("Step Out"),  "debug.stepOut",   "step-out",  nullptr,   Qt::Key_unknown, Qt::NoModifier, false },
        { QT_TR_NOOP("Stop"),      "debug.terminate", "terminate", "#c72e24", Qt::Key_unknown, Qt::NoModifier, true  },
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
            // Icon-only in toolbar; text shows in tooltip and menu.
            act->setToolTip(tr(a.text));
        }
    }

    // F5 and Shift+F5 are registered by BuildPlugin (build.debug / build.stop).
    // Only register step commands here to avoid Qt "ambiguous shortcut" errors.
    // F5 is owned by BuildPlugin (build.debug handles both start + continue).
    // Only register step/continue commands unique to debug plugin.
    addMenuCommands(IMenuManager::Debug, {
        {tr("Step &Over"), QStringLiteral("debug.stepOver"), QKeySequence(Qt::Key_F10), true},
        {tr("Step &Into"), QStringLiteral("debug.stepInto"), QKeySequence(Qt::Key_F11)},
        {tr("Step O&ut"), QStringLiteral("debug.stepOut"), QKeySequence(Qt::SHIFT | Qt::Key_F11)},
        {tr("&Memory View"), QStringLiteral("debug.openMemoryView"), QKeySequence(), true},
        {tr("&Disassembly"), QStringLiteral("debug.openDisassembly"), QKeySequence()},
    }, this);
}

void DebugPlugin::onWorkspaceChanged(const QString &root)
{
    Q_UNUSED(root)
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("debug"), true);
}

#include "debugplugin.moc"
