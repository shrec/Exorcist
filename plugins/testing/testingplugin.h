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

private slots:
    // Cross-DLL slots — m_buildSystem comes from build plugin DLL, so PMF
    // connect would silently fail; use SIGNAL/SLOT.
    void onBuildFinished(bool success, int exitCode);
    void onConfigureFinished(bool success, const QString &message);

private:
    TestDiscoveryService *m_discoverySvc = nullptr;
    TestExplorerPanel    *m_panel        = nullptr;
    TestRunnerService    *m_runnerSvc    = nullptr;
    QObject              *m_buildSys     = nullptr;
};
