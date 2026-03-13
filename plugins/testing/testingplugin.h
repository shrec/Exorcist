#pragma once

#include "plugininterface.h"
#include "plugin/iviewcontributor.h"

class TestDiscoveryService;
class TestExplorerPanel;
class TestRunnerService;

class TestingPlugin : public QObject, public IPlugin, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.exorcist.IPlugin/1.0" FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override {}

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    IHostServices *m_host = nullptr;
    TestDiscoveryService *m_discoverySvc = nullptr;
    TestExplorerPanel    *m_panel        = nullptr;
    TestRunnerService    *m_runnerSvc    = nullptr;
};
