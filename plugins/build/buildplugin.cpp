#include "buildplugin.h"

#include "buildsystemservice.h"
#include "buildtoolbar.h"
#include "cmakeintegration.h"
#include "debuglaunchcontroller.h"
#include "kitmanager.h"
#include "component/outputpanel.h"
#include "component/runlaunchpanel.h"
#include "runconfigdialog.h"
#include "toolchainmanager.h"

#include "sdk/idebugadapter.h"
#include "sdk/ilaunchservice.h"
#include "sdk/icommandservice.h"
#include "sdk/ioutputservice.h"
#include "sdk/irunoutputservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"
#include "sdk/ibuildsystem.h"

#include <QAction>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>

// ── LaunchServiceAdapter ─────────────────────────────────────────────────────
// Adapts DebugLaunchController to the stable ILaunchService SDK interface.

class LaunchServiceAdapter : public ILaunchService
{
    Q_OBJECT

public:
    explicit LaunchServiceAdapter(DebugLaunchController *ctrl, QObject *parent = nullptr)
        : ILaunchService(parent), m_ctrl(ctrl)
    {
        connect(m_ctrl, &DebugLaunchController::processStarted,
                this, &ILaunchService::processStarted);
        connect(m_ctrl, &DebugLaunchController::processFinished,
                this, &ILaunchService::processFinished);
        connect(m_ctrl, &DebugLaunchController::launchError,
                this, &ILaunchService::launchError);
    }

    void startDebugging(const LaunchConfig &config) override
    {
        DebugLaunchConfig dlc;
        dlc.name = config.name;
        dlc.executable = config.executable;
        dlc.args = config.args;
        dlc.workingDir = config.workingDir;
        dlc.env = config.env;
        dlc.debuggerPath = config.debuggerPath;
        m_ctrl->startDebugging(dlc);
    }

    void startWithoutDebugging(const LaunchConfig &config) override
    {
        DebugLaunchConfig dlc;
        dlc.name = config.name;
        dlc.executable = config.executable;
        dlc.args = config.args;
        dlc.workingDir = config.workingDir;
        dlc.env = config.env;
        dlc.debuggerPath = config.debuggerPath;
        m_ctrl->startWithoutDebugging(dlc);
    }

    void stopSession() override
    {
        m_ctrl->stopDebugging();
    }

    bool buildBeforeRun() const override
    {
        return m_ctrl->buildBeforeRun();
    }

    void setBuildBeforeRun(bool enabled) override
    {
        m_ctrl->setBuildBeforeRun(enabled);
    }

private:
    DebugLaunchController *m_ctrl;
};

// ── OutputServiceAdapter ─────────────────────────────────────────────────────
// Adapts OutputPanel to the stable IOutputService SDK interface.
// Uses a callback to show the output dock (avoids coupling to WorkbenchPluginBase).

class OutputServiceAdapter : public IOutputService
{
    Q_OBJECT

public:
    OutputServiceAdapter(OutputPanel *panel,
                         std::function<void()> showDock,
                         QObject *parent = nullptr)
        : IOutputService(parent), m_panel(panel), m_showDock(std::move(showDock))
    {}

    void appendLine(const QString &channel, const QString &text, bool isError) override
    {
        Q_UNUSED(channel)
        m_panel->appendBuildLine(text, isError);
        emit lineAppended(channel, text, isError);
    }

    void appendLine(const QString &text, bool isError) override
    {
        m_panel->appendBuildLine(text, isError);
        emit lineAppended({}, text, isError);
    }

    void clearChannel(const QString &channel) override
    {
        Q_UNUSED(channel)
        m_panel->clear();
        emit channelCleared(channel);
    }

    void clear() override { m_panel->clear(); }

    void show(const QString &channel) override
    {
        Q_UNUSED(channel)
        if (m_showDock)
            m_showDock();
    }

    QString activeChannel() const override { return QStringLiteral("Build"); }

private:
    OutputPanel *m_panel;
    std::function<void()> m_showDock;
};

// ── RunOutputServiceAdapter ──────────────────────────────────────────────────
// Adapts RunLaunchPanel to the stable IRunOutputService SDK interface.

class RunOutputServiceAdapter : public IRunOutputService
{
    Q_OBJECT

public:
    RunOutputServiceAdapter(RunLaunchPanel *panel,
                            std::function<void()> showDock,
                            QObject *parent = nullptr)
        : IRunOutputService(parent), m_panel(panel), m_showDock(std::move(showDock))
    {}

    void appendOutput(const QString &text, bool isError) override
    {
        m_panel->appendOutput(text, isError);
        emit outputAppended(text, isError);
    }

    void clear() override { m_panel->appendOutput(QStringLiteral("\n"), false); }

    void show() override
    {
        if (m_showDock)
            m_showDock();
    }

    void notifyProcessStarted(const QString &executable) override
    {
        m_panel->appendOutput(tr("▶ Started: %1\n").arg(executable), false);
    }

    void notifyProcessFinished(int exitCode) override
    {
        m_panel->appendOutput(
            exitCode == 0
                ? tr("✓ Process exited with code 0\n")
                : tr("✕ Process exited with code %1\n").arg(exitCode),
            exitCode != 0);
    }

private:
    RunLaunchPanel *m_panel;
    std::function<void()> m_showDock;
};

// ── BuildPlugin ──────────────────────────────────────────────────────────────

PluginInfo BuildPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.build"),
        QStringLiteral("Build System"),
        QStringLiteral("1.0.0"),
        QStringLiteral("CMake build system, toolchain detection, output panel, run/launch"),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {PluginPermission::WorkspaceRead, PluginPermission::TerminalExecute, PluginPermission::FilesystemRead}
    };
}

bool BuildPlugin::initializePlugin()
{
    // Create build subsystem objects
    m_toolchainMgr = new ToolchainManager(this);
    m_kitMgr       = new KitManager(m_toolchainMgr, this);
    m_cmake        = new CMakeIntegration(this);
    m_cmake->setToolchainManager(m_toolchainMgr);
    m_output       = new OutputPanel(nullptr);
    m_toolbar      = new BuildToolbar(nullptr);
    m_runPanel     = new RunLaunchPanel(nullptr);

    m_launcher     = new DebugLaunchController(this);
    m_launcher->setCMakeIntegration(m_cmake);
    m_launcher->setToolchainManager(m_toolchainMgr);

    m_toolbar->setCMakeIntegration(m_cmake);
    m_toolbar->setToolchainManager(m_toolchainMgr);

    // Wire debug adapter from core (if already registered)
    wireDebugAdapter();

    wireConnections();

    // Create and register IBuildSystem service
    m_buildSvc = new BuildSystemService(m_cmake, this);
    registerService(QStringLiteral("buildSystem"), m_buildSvc);

    // Register ILaunchService
    auto *launchSvc = new LaunchServiceAdapter(m_launcher, this);
    registerService(QStringLiteral("launchService"), launchSvc);

    // Register kit manager
    registerService(QStringLiteral("kitManager"), m_kitMgr);

    // ── Add toolbar via IToolBarManager (plugin-owned, not MainWindow) ──
    createToolBar(QStringLiteral("build"), tr("Build"));
    addToolBarWidget(QStringLiteral("build"), m_toolbar);

    // Hide build toolbar until a workspace is opened (no project = no build actions)
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("build"), false);

    registerCommands();
    installMenus();

    // Show Output dock when any build/configure output arrives.
    // String-based SIGNAL/SLOT — IBuildSystem is an SDK type whose MOC may
    // be duplicated across DLLs; PMF connect silently fails in that case.
    connect(m_buildSvc, SIGNAL(buildOutput(QString,bool)),
            this, SLOT(onBuildOutputShowDock(QString,bool)));

    // Show Run dock when a non-debug process starts
    connect(m_launcher, &DebugLaunchController::processStarted,
            this, [this]() {
        showPanel(QStringLiteral("RunDock"));
    });

    // Register OutputPanel for ProblemsPanel integration
    registerService(QStringLiteral("outputPanel"), m_output);

    // Register RunLaunchPanel for workspace change notifications
    registerService(QStringLiteral("runPanel"), m_runPanel);

    // ── SDK service adapters (stable inter-plugin interfaces) ────────────
    auto *outputSvc = new OutputServiceAdapter(
        m_output, [this]() { showPanel(QStringLiteral("OutputDock")); }, this);
    registerService(QStringLiteral("outputService"), outputSvc);

    auto *runOutputSvc = new RunOutputServiceAdapter(
        m_runPanel, [this]() { showPanel(QStringLiteral("RunDock")); }, this);
    registerService(QStringLiteral("runOutputService"), runOutputSvc);

    // Set working directory from workspace
    const QString root = workspaceRoot();
    if (!root.isEmpty())
        setWorkingDir(root);

    // Auto-detect kits (async)
    m_kitMgr->detectKits();

    return true;
}

void BuildPlugin::shutdownPlugin()
{
    // Views are owned by dock manager (parented away from us) — nothing to do.
}

QWidget *BuildPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("OutputDock") && m_output) {
        m_output->setParent(parent);
        return m_output;
    }
    if (viewId == QLatin1String("RunDock") && m_runPanel) {
        m_runPanel->setParent(parent);
        return m_runPanel;
    }
    return nullptr;
}

// ── Internal ─────────────────────────────────────────────────────────────────

void BuildPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("build.configure"), tr("Configure"), [this]() {
        if (m_cmake)
            m_cmake->configure();
    });

    cmds->registerCommand(QStringLiteral("build.build"), tr("Build"), [this]() {
        if (m_cmake)
            m_cmake->build();
    });

    cmds->registerCommand(QStringLiteral("build.clean"), tr("Clean"), [this]() {
        if (m_cmake)
            m_cmake->clean();
    });

    cmds->registerCommand(QStringLiteral("build.run"), tr("Run Without Debugging"), [this]() {
        const QString exe = m_toolbar ? m_toolbar->selectedTarget() : QString();
        // exe may be empty; DebugLaunchController auto-discovers after build
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startWithoutDebugging(cfg);
    });

    cmds->registerCommand(QStringLiteral("build.debug"), tr("Start Debugging"), [this]() {
        // Lazy wire — debug plugin may have initialized after build plugin.
        if (!m_debugAdapter)
            wireDebugAdapter();

        const bool hasAdapter = (m_debugAdapter != nullptr);
        const bool isRunning  = hasAdapter && m_debugAdapter->isRunning();
        qDebug() << "[BuildPlugin] build.debug fired — adapter=" << hasAdapter
                 << "isRunning=" << isRunning
                 << "launcher=" << (m_launcher ? "yes" : "NULL");

        if (hasAdapter && isRunning) {
            qDebug() << "[BuildPlugin]   → continueExecution";
            m_debugAdapter->continueExecution();
            return;
        }

        const QString exe = m_toolbar ? m_toolbar->selectedTarget() : QString();
        qDebug() << "[BuildPlugin]   → startDebugging exe=" << exe << "wd=" << m_workingDir;
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startDebugging(cfg);
    });

    cmds->registerCommand(QStringLiteral("build.stop"), tr("Stop"), [this]() {
        if (!m_debugAdapter)
            wireDebugAdapter();
        if (m_launcher)
            m_launcher->stopDebugging();
    });

    // ── Edit Run Configuration ──────────────────────────────────────────
    // Per-target persistence of program args, environment block, and cwd.
    // Persisted via QSettings under "runConfig/<target>". The actual launch
    // flow is not modified yet — values are stored for future use.
    // TODO: read these values back in the run/debug launch path
    //       (BuildToolbar::runRequested / debugRequested handlers above) so
    //       the configured args/env/cwd are applied to DebugLaunchConfig.
    cmds->registerCommand(QStringLiteral("build.editRunConfig"),
                          tr("Edit Run Configuration..."), [this]() {
        const QString target = m_toolbar ? m_toolbar->selectedTarget() : QString();
        if (target.isEmpty()) {
            showInfo(tr("No target selected"));
            return;
        }
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.beginGroup(QStringLiteral("runConfig/") + target);
        const QString args = s.value(QStringLiteral("args")).toString();
        const QString env  = s.value(QStringLiteral("env")).toString();
        const QString cwd  = s.value(QStringLiteral("cwd")).toString();
        s.endGroup();

        RunConfigDialog dlg(args, env, cwd);
        if (dlg.exec() == QDialog::Accepted) {
            s.beginGroup(QStringLiteral("runConfig/") + target);
            s.setValue(QStringLiteral("args"), dlg.args());
            s.setValue(QStringLiteral("env"),  dlg.envBlock());
            s.setValue(QStringLiteral("cwd"),  dlg.workingDir());
            s.setValue(QStringLiteral("externalTerminal"), dlg.useExternalTerminal());
            s.endGroup();
        }
    });
}

void BuildPlugin::installMenus()
{
    auto *menuMgr = menus();
    if (!menuMgr)
        return;

    addMenuCommand(IMenuManager::Build, tr("&Configure"),
                   QStringLiteral("build.configure"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    addMenuCommand(IMenuManager::Build, tr("&Build"),
                   QStringLiteral("build.build"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    addMenuCommand(IMenuManager::Build, tr("C&lean"),
                   QStringLiteral("build.clean"), this);

    addMenuSeparator(IMenuManager::Build);

    addMenuCommand(IMenuManager::Build, tr("Edit Run Configuration..."),
                   QStringLiteral("build.editRunConfig"), this);

    addMenuSeparator(IMenuManager::Build);

    addMenuCommand(IMenuManager::Debug, tr("Start &Debugging"),
                   QStringLiteral("build.debug"), this,
                   QKeySequence(Qt::Key_F5));
    addMenuCommand(IMenuManager::Debug, tr("Run &Without Debugging"),
                   QStringLiteral("build.run"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_F5));

    addMenuSeparator(IMenuManager::Debug);

    addMenuCommand(IMenuManager::Debug, tr("&Stop"),
                   QStringLiteral("build.stop"), this,
                   QKeySequence(Qt::SHIFT | Qt::Key_F5));
}

void BuildPlugin::wireDebugAdapter()
{
    if (m_debugAdapter) return;  // already wired
    QObject *raw = queryService(QStringLiteral("debugAdapter"));
    auto *adapter = qobject_cast<IDebugAdapter *>(raw);
    qDebug() << "[BuildPlugin::wireDebugAdapter] raw=" << raw
             << "qobject_cast<IDebugAdapter>=" << adapter
             << "(if raw is non-null but cast yields null, dual-MOC of IDebugAdapter — qobject_cast fails across DLL boundary)";
    if (!adapter) return;

    m_debugAdapter = adapter;
    m_launcher->setDebugAdapter(adapter);
    m_toolbar->setDebugAdapter(adapter);

    // Use SIGNAL/SLOT (string-based) for cross-DLL connections.
    // PMF connect (& syntax) silently fails across DLL boundaries because each
    // plugin DLL has its own MOC copy of IDebugAdapter with different addresses.
    connect(adapter, SIGNAL(error(QString)),
            this, SLOT(onAdapterError(QString)));
    connect(adapter, SIGNAL(outputProduced(QString,QString)),
            this, SLOT(onAdapterOutput(QString,QString)));
}

void BuildPlugin::onBuildOutputShowDock(const QString & /*line*/, bool /*isError*/)
{
    showPanel(QStringLiteral("OutputDock"));
}

void BuildPlugin::onAdapterOutput(const QString &text, const QString &category)
{
    if (category == QStringLiteral("debug") || category == QStringLiteral("stderr")
            || category == QStringLiteral("console") || category == QStringLiteral("command"))
        m_output->appendBuildLine(text.trimmed(), category == QStringLiteral("stderr"));
}

void BuildPlugin::onAdapterError(const QString &msg)
{
    m_output->appendBuildLine(QStringLiteral("[GDB] ") + msg, /*isError=*/true);
}

void BuildPlugin::wireConnections()
{
    // ── Build toolbar → CMake lifecycle ───────────────────────────────────
    connect(m_toolbar, &BuildToolbar::configureRequested, this, [this]() {
        showPanel(QStringLiteral("OutputDock"));  // show output immediately on action
        m_output->clear();
        m_cmake->configure();
    });

    connect(m_cmake, &CMakeIntegration::configureFinished,
            this, [this](bool success, const QString &msg) {
        if (!success) {
            // Show early-out errors (no CMakeLists.txt, cmake not found, etc.)
            if (!msg.isEmpty())
                m_output->appendBuildLine(msg, /*isError=*/true);
            return;
        }
        m_toolbar->refresh();

        // Notify LSP (clangd) that compile_commands.json may have changed.
        // This triggers IntelliSense to become active for the project.
        const QString ccPath = m_cmake->compileCommandsPath();
        if (!ccPath.isEmpty()) {
            if (auto *lsp = queryService(QStringLiteral("lspService"))) {
                QMetaObject::invokeMethod(lsp, "reloadCompileCommands",
                                         Qt::QueuedConnection,
                                         Q_ARG(QString, ccPath));
            }
        }
    });

    connect(m_toolbar, &BuildToolbar::buildRequested, this, [this]() {
        showPanel(QStringLiteral("OutputDock"));  // show output immediately on action
        m_output->clear();
        m_cmake->build();
    });

    connect(m_toolbar, &BuildToolbar::stopRequested, this, [this]() {
        if (m_cmake->isBuilding())
            m_cmake->cancelBuild();
        m_launcher->stopDebugging();
    });

    connect(m_toolbar, &BuildToolbar::cleanRequested, this, [this]() {
        showPanel(QStringLiteral("OutputDock"));
        m_output->clear();
        m_cmake->clean();
    });

    // ── CMake output → Output panel ──────────────────────────────────────
    connect(m_cmake, &CMakeIntegration::buildOutput,
            this, [this](const QString &line, bool isError) {
        m_output->appendBuildLine(line, isError);
    });

    // ── Post-build: refresh target list in toolbar ───────────────────────
    connect(m_cmake, &CMakeIntegration::buildFinished,
            this, [this](bool success, int) {
        if (success)
            m_toolbar->refresh();
    });

    // ── Run (Ctrl+F5) ────────────────────────────────────────────────────
    connect(m_toolbar, &BuildToolbar::runRequested,
            this, [this](const QString &exe) {
        DebugLaunchConfig cfg;
        cfg.executable = exe;  // may be empty; DebugLaunchController will auto-discover after build
        cfg.workingDir = m_workingDir;
        m_launcher->startWithoutDebugging(cfg);
    });

    // ── Debug (F5) ───────────────────────────────────────────────────────
    connect(m_toolbar, &BuildToolbar::debugRequested,
            this, [this](const QString &exe) {
        wireDebugAdapter();  // lazy — debug plugin may activate after build plugin
        m_output->appendBuildLine(QStringLiteral("[DEBUG] debugRequested: exe=%1 adapter=%2")
                 .arg(exe.isEmpty() ? QStringLiteral("(empty)") : exe,
                      m_debugAdapter ? QStringLiteral("yes") : QStringLiteral("NULL")), false);
        DebugLaunchConfig cfg;
        cfg.executable = exe;  // may be empty; DebugLaunchController will auto-discover after build
        cfg.workingDir = m_workingDir;
        m_launcher->startDebugging(cfg);
    });

    // ── Process output → Run panel ──────────────────────────────────────
    connect(m_launcher, &DebugLaunchController::processOutput,
            this, [this](const QString &text, bool isError) {
        showPanel(QStringLiteral("RunDock"));
        m_runPanel->appendOutput(text, isError);
    });

    // ── Debug log → output panel ────────────────────────────────────────
    connect(m_launcher, &DebugLaunchController::debugLog,
            this, [this](const QString &msg) {
        m_output->appendBuildLine(msg, false);
    });

    // ── Launch errors → notification ─────────────────────────────────────
    connect(m_launcher, &DebugLaunchController::launchError,
            this, [this](const QString &msg) {
        showInfo(tr("Launch error: %1").arg(msg));
    });
}

void BuildPlugin::onWorkspaceChanged(const QString &root)
{
    // Show build toolbar and output dock now that a project is open
    if (toolbars())
        toolbars()->setVisible(QStringLiteral("build"), true);
    showPanel(QStringLiteral("OutputDock"));

    setWorkingDir(root);

    // Load workspace task/launch configs if present.
    if (m_output) {
        const QString tasksPath = root + QStringLiteral("/.exorcist/tasks.json");
        if (QFileInfo::exists(tasksPath))
            m_output->loadTasksFromJson(tasksPath);
    }
    if (m_runPanel) {
        const QString launchPath = root + QStringLiteral("/.exorcist/launch.json");
        if (QFileInfo::exists(launchPath))
            m_runPanel->loadLaunchJson(launchPath);
    }
}

void BuildPlugin::setWorkingDir(const QString &dir)
{
    m_workingDir = dir;

    // Tell CMakeIntegration where the project lives and discover build configs.
    // This is the critical step that makes Configure/Build work — without it
    // cmake doesn't know the workspace root and rejects all operations.
    if (m_cmake) {
        m_cmake->setProjectRoot(dir);
        m_cmake->autoDetectConfigs();
    }

    if (m_output)  m_output->setWorkingDirectory(dir);
    if (m_runPanel) m_runPanel->setWorkingDirectory(dir);

    // Refresh toolbar combos (config list + target list) after config change.
    if (m_toolbar) m_toolbar->refresh();
}

#include "buildplugin.moc"
