#include "testingplugin.h"

#include "testdiscoveryservice.h"
#include "testexplorerpanel.h"
#include "testrunnerservice.h"

#include "sdk/ibuildsystem.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"

PluginInfo TestingPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.testing"),
        QStringLiteral("Testing"),
        QStringLiteral("1.0.0"),
        QStringLiteral("CTest-based test discovery, execution, and explorer panel"),
        QStringLiteral("Exorcist Team")
    };
}

bool TestingPlugin::initializePlugin()
{
    m_discoverySvc = new TestDiscoveryService(this);
    m_panel        = new TestExplorerPanel(nullptr);
    m_panel->setDiscoveryService(m_discoverySvc);

    m_runnerSvc = new TestRunnerService(m_discoverySvc, this);
    registerService(QStringLiteral("testRunner"), m_runnerSvc);

    wireBuildSystem();
    registerCommands();
    installMenusAndToolBar();

    connect(m_discoverySvc, &TestDiscoveryService::discoveryFinished,
            this, [this]() {
        if (!m_discoverySvc->tests().isEmpty())
            showPanel(QStringLiteral("TestExplorerDock"));
    });

    return true;
}

QWidget *TestingPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("TestExplorerDock")) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

void TestingPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("testing.discover"), tr("Discover Tests"), [this]() {
        showPanel(QStringLiteral("TestExplorerDock"));
        if (m_discoverySvc)
            m_discoverySvc->discoverTests();
    });

    cmds->registerCommand(QStringLiteral("testing.runAll"), tr("Run All Tests"), [this]() {
        showPanel(QStringLiteral("TestExplorerDock"));

        if (!m_discoverySvc)
            return;

        if (m_discoverySvc->tests().isEmpty())
            m_discoverySvc->discoverTests();
        m_discoverySvc->runAllTests();
    });
}

void TestingPlugin::installMenusAndToolBar()
{
    createToolBar(QStringLiteral("testing"), tr("Test"));
    addToolBarCommands(QStringLiteral("testing"), {
        {tr("Discover"), QStringLiteral("testing.discover")},
        {tr("Run All"), QStringLiteral("testing.runAll")},
    }, this);

    addMenuCommands(IMenuManager::Test, {
        {tr("&Discover Tests"), QStringLiteral("testing.discover")},
        {tr("&Run All Tests"), QStringLiteral("testing.runAll")},
    }, this);
}

void TestingPlugin::wireBuildSystem()
{
    auto *buildSys = service<IBuildSystem>(QStringLiteral("buildSystem"));
    if (!buildSys)
        return;

    const QString buildDir = buildSys->buildDirectory();
    if (!buildDir.isEmpty()) {
        m_discoverySvc->setBuildDirectory(buildDir);
        m_discoverySvc->discoverTests();
    }

    // SIGNAL/SLOT string-based connect — buildSys lives in another DLL.
    m_buildSys = buildSys;
    connect(buildSys, SIGNAL(buildFinished(bool,int)),
            this, SLOT(onBuildFinished(bool,int)));
    connect(buildSys, SIGNAL(configureFinished(bool,QString)),
            this, SLOT(onConfigureFinished(bool,QString)));
}

void TestingPlugin::onBuildFinished(bool success, int /*exitCode*/)
{
    if (success && m_discoverySvc && !m_discoverySvc->buildDirectory().isEmpty())
        m_discoverySvc->discoverTests();
}

void TestingPlugin::onConfigureFinished(bool success, const QString & /*message*/)
{
    if (!success || !m_discoverySvc || !m_buildSys) return;

    auto *buildSys = qobject_cast<IBuildSystem *>(m_buildSys);
    if (!buildSys) return;

    const QString buildDir = buildSys->buildDirectory();
    if (!buildDir.isEmpty())
        m_discoverySvc->setBuildDirectory(buildDir);

    if (!m_discoverySvc->buildDirectory().isEmpty())
        m_discoverySvc->discoverTests();
}
