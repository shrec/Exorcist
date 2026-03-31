#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class ServiceDiscovery;
class DevOpsPanel;

class DevOpsPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit DevOpsPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    void registerCommands();

    ServiceDiscovery *m_discovery = nullptr;
    DevOpsPanel      *m_panel    = nullptr;
    QString           m_workDir;
};
