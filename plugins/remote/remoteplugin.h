#pragma once

#include <QObject>

#include "plugininterface.h"
#include "plugin/iviewcontributor.h"

class SshConnectionManager;
class RemoteSyncService;
class IHostServices;

class RemotePlugin : public QObject, public IPlugin, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    IHostServices *m_host = nullptr;
    SshConnectionManager *m_connMgr = nullptr;
    RemoteSyncService *m_syncService = nullptr;
};
