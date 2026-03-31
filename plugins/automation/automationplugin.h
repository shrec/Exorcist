#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class TaskDiscovery;
class TaskRunnerPanel;

class AutomationPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit AutomationPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    void registerCommands();

    TaskDiscovery   *m_discovery = nullptr;
    TaskRunnerPanel *m_panel     = nullptr;
    QString          m_workDir;
};
