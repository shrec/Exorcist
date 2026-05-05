#include "debugplugin.h"

#include "gdbmiadapter.h"
#include "memoryviewpanel.h"
#include "disassemblypanel.h"
#include "debugpanel.h"
#include "ui/themeicons.h"

#include "sdk/idebugadapter.h"
#include "sdk/idebugservice.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"

#include "editor/editormanager.h"
#include "editor/editorview.h"

#include <QAction>
#include <QTabWidget>
#include <QDebug>
#include <QHash>
#include <QSettings>

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

    // ── Editor breakpoint integration (rules L1, L5) ─────────────────────
    // Pull breakpoint sync out of MainWindow / PostPluginBootstrap:
    // debug plugin owns the wiring entirely.  Each open editor's
    // breakpointToggled / ConditionChanged / EnabledChanged signal flows
    // through the plugin to the GDB adapter; new editors are picked up
    // via EditorManager::editorOpened.
    m_editorMgr = service<EditorManager>(QStringLiteral("editorManager"));
    if (m_editorMgr) {
        connect(m_editorMgr, &EditorManager::editorOpened,
                this, &DebugPlugin::onEditorOpenedWireBreakpoints);
        wireExistingEditors();
    }

    return true;
}

// ── Editor breakpoint sync (rules L1, L5) ────────────────────────────────────
//
// Owns the entire breakpoint-gutter ↔ GDB adapter pipeline.  Previously
// duplicated across MainWindow and PostPluginBootstrap; now centralized here
// so MainWindow has zero knowledge of IDebugAdapter or DebugBreakpoint.

void DebugPlugin::onEditorOpenedWireBreakpoints(EditorView *editor, const QString &)
{
    if (!editor) return;
    connect(editor, &EditorView::breakpointToggled,
            this, &DebugPlugin::onEditorBreakpointToggled,
            Qt::UniqueConnection);
    connect(editor, &EditorView::breakpointConditionChanged,
            this, &DebugPlugin::onEditorBreakpointConditionChanged,
            Qt::UniqueConnection);
    connect(editor, &EditorView::breakpointEnabledChanged,
            this, &DebugPlugin::onEditorBreakpointEnabledChanged,
            Qt::UniqueConnection);
}

void DebugPlugin::wireExistingEditors()
{
    if (!m_editorMgr || !m_editorMgr->tabs()) return;
    auto *tabs = m_editorMgr->tabs();
    for (int i = 0; i < tabs->count(); ++i) {
        auto *editor = m_editorMgr->editorAt(i);
        if (!editor) continue;
        const QString fp = editor->property("filePath").toString();
        // Wire signals so future toggles propagate.
        onEditorOpenedWireBreakpoints(editor, fp);
        // Replay any pre-existing breakpoint markers to the adapter so the
        // upcoming debug session honors them.  (Adapter queues if not yet
        // launched.)
        if (m_adapter && !fp.isEmpty()) {
            for (int line : editor->breakpointLines()) {
                DebugBreakpoint bp;
                bp.filePath  = fp;
                bp.line      = line;
                bp.condition = editor->breakpointCondition(line);
                bp.enabled   = editor->isBreakpointEnabled(line);
                m_adapter->addBreakpoint(bp);
            }
        }
    }
}

void DebugPlugin::onEditorBreakpointToggled(const QString &fp, int line, bool added)
{
    if (!m_adapter) return;
    if (added) {
        if (m_debugService)
            m_debugService->addBreakpointEntry(fp, line);
        DebugBreakpoint bp;
        bp.filePath = fp;
        bp.line = line;
        m_adapter->addBreakpoint(bp);
    } else {
        if (m_debugService) {
            const int bpId = m_debugService->breakpointIdForLocation(fp, line);
            if (m_adapter->isRunning() && bpId >= 0)
                m_adapter->removeBreakpoint(bpId);
            m_debugService->removeBreakpointEntry(fp, line);
        }
    }
}

void DebugPlugin::onEditorBreakpointConditionChanged(const QString &fp, int line,
                                                     const QString &condition)
{
    if (!m_adapter || fp.isEmpty()) return;
    if (m_debugService) {
        const int oldId = m_debugService->breakpointIdForLocation(fp, line);
        if (oldId >= 0)
            m_adapter->removeBreakpoint(oldId);
    }
    DebugBreakpoint bp;
    bp.filePath  = fp;
    bp.line      = line;
    bp.condition = condition;
    // enabled state must come from the originating editor — query via
    // sender() so we don't have to track it on the plugin side.
    if (auto *ed = qobject_cast<EditorView *>(sender()))
        bp.enabled = ed->isBreakpointEnabled(line);
    else
        bp.enabled = true;
    m_adapter->addBreakpoint(bp);
}

void DebugPlugin::onEditorBreakpointEnabledChanged(const QString &fp, int line, bool enabled)
{
    if (!m_adapter || fp.isEmpty()) return;
    if (m_debugService) {
        const int oldId = m_debugService->breakpointIdForLocation(fp, line);
        if (oldId >= 0)
            m_adapter->removeBreakpoint(oldId);
    }
    DebugBreakpoint bp;
    bp.filePath  = fp;
    bp.line      = line;
    if (auto *ed = qobject_cast<EditorView *>(sender()))
        bp.condition = ed->breakpointCondition(line);
    bp.enabled   = enabled;
    m_adapter->addBreakpoint(bp);
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
        qDebug() << "[DebugPlugin] debugCmd(" << name << ") adapter=" << hasAdapter
                 << "isRunning=" << isRunning
                 << "→" << (hasAdapter && isRunning ? "fire" : "SKIP");
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

    // Toggle "stop on uncaught exceptions" — installs GDB `catch throw` /
    // `catch rethrow` catchpoints when enabled. The preference is persisted
    // via QSettings so it survives across debug sessions; GdbMiAdapter
    // re-applies it from QSettings in onProcessStarted().
    cmds->registerCommand(QStringLiteral("debug.toggleStopOnExceptions"),
                          tr("Stop on Exceptions"), [this]() {
        if (!m_adapter)
            return;
        const bool enable = !m_adapter->stopOnExceptions();
        m_adapter->setStopOnExceptions(enable);
        QSettings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"))
            .setValue(QStringLiteral("debug/stopOnExceptions"), enable);
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

    // F5/Shift+F5 are registered by BuildPlugin (build.debug / build.stop).
    // F10/F11/Shift+F11 are registered as command shortcuts by
    // GlobalShortcutDispatcher in MainWindow::deferredInit() — installing
    // the SAME key here as a QAction shortcut creates an ambiguous-shortcut
    // overload that Qt silently refuses to fire (the visible "F10 doesn't
    // step" regression).  We pass empty QKeySequences here so the menu
    // shows "Step Over" without a competing accelerator; the dispatcher
    // remains the sole owner of those keys.  The "F10" text hint is added
    // manually below so the menu still tells the user what to press.
    addMenuCommands(IMenuManager::Debug, {
        {tr("Step &Over\tF10"), QStringLiteral("debug.stepOver"), QKeySequence(), true},
        {tr("Step &Into\tF11"), QStringLiteral("debug.stepInto"), QKeySequence()},
        {tr("Step O&ut\tShift+F11"), QStringLiteral("debug.stepOut"), QKeySequence()},
        {tr("&Memory View"), QStringLiteral("debug.openMemoryView"), QKeySequence(), true},
        {tr("&Disassembly"), QStringLiteral("debug.openDisassembly"), QKeySequence()},
    }, this);

    // "Stop on Exceptions" toggle — installs GDB catch throw / catch rethrow
    // when enabled. Persisted via QSettings under debug/stopOnExceptions.
    addMenuCommand(IMenuManager::Debug, tr("Stop on E&xceptions"),
                   QStringLiteral("debug.toggleStopOnExceptions"), this);
}

void DebugPlugin::onWorkspaceChanged(const QString &root)
{
    Q_UNUSED(root)
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("debug"), true);
}

#include "debugplugin.moc"
