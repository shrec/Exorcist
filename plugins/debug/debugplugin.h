#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class GdbMiAdapter;
class DebugPanel;
class MemoryViewPanel;
class IDebugService;

class DebugPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit DebugPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private slots:
    void onAdapterStartedShowDock();
    void onDebugStoppedShowDock();
    void onAdapterErrorShowInfo(const QString &msg);

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    void onWorkspaceChanged(const QString &root) override;
    void registerCommands();
    void installMenusAndToolBar();

    GdbMiAdapter    *m_adapter        = nullptr;
    DebugPanel      *m_panel          = nullptr;
    MemoryViewPanel *m_memoryView     = nullptr;
    IDebugService   *m_debugService   = nullptr;
};
