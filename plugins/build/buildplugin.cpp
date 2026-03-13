#include "buildplugin.h"

#include "buildsystemservice.h"
#include "buildtoolbar.h"
#include "cmakeintegration.h"
#include "debuglaunchcontroller.h"
#include "build/outputpanel.h"
#include "build/runlaunchpanel.h"
#include "toolchainmanager.h"

#include "debug/idebugadapter.h"
#include "sdk/ihostservices.h"
#include "sdk/ilaunchservice.h"
#include "sdk/inotificationservice.h"
#include "sdk/iworkspaceservice.h"

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

bool BuildPlugin::initialize(IHostServices *host)
{
    m_host = host;

    // Create build subsystem objects
    m_toolchainMgr = new ToolchainManager(this);
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
    m_host->registerService(QStringLiteral("buildSystem"), m_buildSvc);

    // Register ILaunchService
    auto *launchSvc = new LaunchServiceAdapter(m_launcher, this);
    m_host->registerService(QStringLiteral("launchService"), launchSvc);

    // Register the toolbar widget as a service so MainWindow can add it
    m_host->registerService(QStringLiteral("buildToolbar"), m_toolbar);

    // Register OutputPanel for ProblemsPanel integration
    m_host->registerService(QStringLiteral("outputPanel"), m_output);

    // Register RunLaunchPanel for workspace change notifications
    m_host->registerService(QStringLiteral("runPanel"), m_runPanel);

    // Set working directory from workspace
    if (m_host->workspace()) {
        const QString root = m_host->workspace()->rootPath();
        if (!root.isEmpty())
            setWorkingDir(root);
    }

    return true;
}

void BuildPlugin::shutdown()
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

void BuildPlugin::wireDebugAdapter()
{
    if (!m_host) return;
    auto *adapter = qobject_cast<IDebugAdapter *>(m_host->queryService(QStringLiteral("debugAdapter")));
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
        if (m_host && m_host->notifications())
            m_host->notifications()->info(tr("Launch error: %1").arg(msg));
    });
}

void BuildPlugin::setWorkingDir(const QString &dir)
{
    m_workingDir = dir;
    if (m_output) m_output->setWorkingDirectory(dir);
    if (m_runPanel) m_runPanel->setWorkingDirectory(dir);
}

#include "buildplugin.moc"
