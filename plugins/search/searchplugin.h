#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class SearchPanel;
class WorkspaceIndexer;
class SymbolIndex;

class SearchPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit SearchPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

    /// Workspace lifecycle hooks (rule L2).
    void onWorkspaceOpened(const QString &root) override;
    void onWorkspaceClosed() override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    SearchPanel      *m_panel       = nullptr;
    WorkspaceIndexer *m_indexer     = nullptr;
    SymbolIndex      *m_symbolIndex = nullptr;
};
