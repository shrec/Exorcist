#include "testingplugin.h"

#include "testdiscoveryservice.h"
#include "testexplorerpanel.h"
#include "testrunnerservice.h"

#include "sdk/ihostservices.h"
#include "sdk/ibuildsystem.h"

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

bool TestingPlugin::initialize(IHostServices *host)
{
    m_host = host;

    m_discoverySvc = new TestDiscoveryService(this);
    m_panel        = new TestExplorerPanel(nullptr);
    m_panel->setDiscoveryService(m_discoverySvc);

    m_runnerSvc = new TestRunnerService(m_discoverySvc, this);
    m_host->registerService(QStringLiteral("testRunner"), m_runnerSvc);

    // Wire build directory from IBuildSystem service (if available)
    auto *buildSys = host->service<IBuildSystem>(QStringLiteral("buildSystem"));
    if (buildSys) {
        const QString buildDir = buildSys->buildDirectory();
        if (!buildDir.isEmpty()) {
            m_discoverySvc->setBuildDirectory(buildDir);
            m_discoverySvc->discoverTests();
        }

        // Re-discover when build finishes (new tests may appear)
        connect(buildSys, &IBuildSystem::buildFinished,
                this, [this](bool success, int) {
            if (success && !m_discoverySvc->buildDirectory().isEmpty())
                m_discoverySvc->discoverTests();
        });

        // Re-discover when configure finishes (may change test list)
        connect(buildSys, &IBuildSystem::configureFinished,
                this, [this](bool success, const QString &) {
            if (success && !m_discoverySvc->buildDirectory().isEmpty())
                m_discoverySvc->discoverTests();
        });
    }

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

#include "testingplugin.moc"
