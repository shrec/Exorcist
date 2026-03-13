#pragma once

#include <QObject>
#include "plugininterface.h"
#include "plugin/iviewcontributor.h"

class GdbMiAdapter;
class DebugPanel;
class IHostServices;
class IDebugService;

class DebugPlugin : public QObject, public IPlugin, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit DebugPlugin(QObject *parent = nullptr);

    // IPlugin
    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    IHostServices  *m_host           = nullptr;
    GdbMiAdapter   *m_adapter        = nullptr;
    DebugPanel     *m_panel          = nullptr;
    IDebugService  *m_debugService   = nullptr;
};
