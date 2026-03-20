#include "buildplugin.h"

#include "buildsystemservice.h"
#include "buildtoolbar.h"
#include "cmakeintegration.h"
#include "debuglaunchcontroller.h"
#include "kitmanager.h"
#include "build/outputpanel.h"
#include "build/runlaunchpanel.h"
#include "toolchainmanager.h"

#include "sdk/idebugadapter.h"
#include "sdk/ilaunchservice.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"
#include "sdk/ibuildsystem.h"

#include <QAction>
#include <QFileInfo>

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
        {}
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

    registerCommands();
    installMenus();

    // ── Auto-show docks ──
    // Plugin owns its own UI lifecycle: docks are shown by the plugin
    // itself, never by MainWindow. This works regardless of trigger source
    // (toolbar button, Run menu, command palette, keybinding).
    // Show Output dock when plugin activates for a build workspace
    showPanel(QStringLiteral("OutputDock"));

    // Show Output dock when any build/configure output arrives
    connect(m_buildSvc, &IBuildSystem::buildOutput, this, [this](const QString &, bool) {
        showPanel(QStringLiteral("OutputDock"));
    });

    // Show Run dock when a non-debug process starts
    connect(m_launcher, &DebugLaunchController::processStarted,
            this, [this]() {
        showPanel(QStringLiteral("RunDock"));
    });

    // Register OutputPanel for ProblemsPanel integration
    registerService(QStringLiteral("outputPanel"), m_output);

    // Register RunLaunchPanel for workspace change notifications
    registerService(QStringLiteral("runPanel"), m_runPanel);

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
    if (viewId == QLatin1String("OutputDock")) {
        m_output->setParent(parent);
        return m_output;
    }
    if (viewId == QLatin1String("RunDock")) {
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
        if (exe.isEmpty())
            return;

        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startWithoutDebugging(cfg);
    });

    cmds->registerCommand(QStringLiteral("build.debug"), tr("Start Debugging"), [this]() {
        if (m_debugAdapter && m_debugAdapter->isRunning()) {
            m_debugAdapter->continueExecution();
            return;
        }

        const QString exe = m_toolbar ? m_toolbar->selectedTarget() : QString();
        if (exe.isEmpty())
            return;

        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startDebugging(cfg);
    });

    cmds->registerCommand(QStringLiteral("build.stop"), tr("Stop"), [this]() {
        if (m_launcher)
            m_launcher->stopDebugging();
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
    auto *adapter = qobject_cast<IDebugAdapter *>(queryService(QStringLiteral("debugAdapter")));
    if (!adapter) return;

    m_debugAdapter = adapter;
    m_launcher->setDebugAdapter(adapter);
    m_toolbar->setDebugAdapter(adapter);
}

void BuildPlugin::wireConnections()
{
    // ── Build toolbar → CMake lifecycle ───────────────────────────────────
    connect(m_toolbar, &BuildToolbar::configureRequested, this, [this]() {
        m_output->clear();
        m_cmake->configure();
    });

    connect(m_cmake, &CMakeIntegration::configureFinished,
            this, [this](bool success, const QString &) {
        if (!success) return;
        m_toolbar->refresh();
    });

    connect(m_toolbar, &BuildToolbar::buildRequested, this, [this]() {
        m_output->clear();
        m_cmake->build();
    });

    connect(m_toolbar, &BuildToolbar::stopRequested, this, [this]() {
        if (m_cmake->isBuilding())
            m_cmake->cancelBuild();
        m_launcher->stopDebugging();
    });

    connect(m_toolbar, &BuildToolbar::cleanRequested, this, [this]() {
        m_cmake->clean();
    });

    // ── CMake output → Output panel ──────────────────────────────────────
    connect(m_cmake, &CMakeIntegration::buildOutput,
            this, [this](const QString &line, bool isError) {
        m_output->appendBuildLine(line, isError);
    });

    // ── Run (Ctrl+F5) ────────────────────────────────────────────────────
    connect(m_toolbar, &BuildToolbar::runRequested,
            this, [this](const QString &exe) {
        if (exe.isEmpty()) return;
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startWithoutDebugging(cfg);
    });

    // ── Debug (F5) ───────────────────────────────────────────────────────
    connect(m_toolbar, &BuildToolbar::debugRequested,
            this, [this](const QString &exe) {
        if (exe.isEmpty()) return;
        DebugLaunchConfig cfg;
        cfg.executable = exe;
        cfg.workingDir = m_workingDir;
        m_launcher->startDebugging(cfg);
    });

    // ── Launch errors → notification ─────────────────────────────────────
    connect(m_launcher, &DebugLaunchController::launchError,
            this, [this](const QString &msg) {
        showInfo(tr("Launch error: %1").arg(msg));
    });
}

void BuildPlugin::setWorkingDir(const QString &dir)
{
    m_workingDir = dir;
    if (m_output) m_output->setWorkingDirectory(dir);
    if (m_runPanel) m_runPanel->setWorkingDirectory(dir);
}

#include "buildplugin.moc"
