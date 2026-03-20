#pragma once

#include <QObject>

#include "plugininterface.h"
#include "plugin/workbenchpluginbase.h"
#include "plugin/iviewcontributor.h"

class SshConnectionManager;
class RemoteSyncService;
class RemoteFilePanel;

class RemotePlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

protected:
    bool initializePlugin() override;
    void shutdownPlugin() override;

private:
    void registerCommands();
    void installMenusAndToolBar();
    void wirePanelSignals();

    SshConnectionManager *m_connMgr = nullptr;
    RemoteSyncService *m_syncService = nullptr;
    RemoteFilePanel *m_panel = nullptr;
};
