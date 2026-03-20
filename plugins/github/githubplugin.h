#pragma once

#include <QObject>

#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class GhService;
class GhPanel;

class GitHubPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;
    void registerCommands();
    void installMenusAndToolBar();

    GhService *m_service = nullptr;
    GhPanel *m_panel = nullptr;
};
