#pragma once

#include <QObject>

#include "plugininterface.h"
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class TestDiscoveryService;
class TestExplorerPanel;
class TestRunnerService;

class TestingPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.exorcist.IPlugin/1.0" FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override {}
    void registerCommands();
    void installMenusAndToolBar();
    void wireBuildSystem();

    TestDiscoveryService *m_discoverySvc = nullptr;
    TestExplorerPanel    *m_panel        = nullptr;
    TestRunnerService    *m_runnerSvc    = nullptr;
};
