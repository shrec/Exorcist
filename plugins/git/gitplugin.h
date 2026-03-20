#pragma once

#include <QObject>
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"
#include "agent/iagentplugin.h"

#include <memory>
#include <vector>

class ITool;
class GitService;
class GitPanel;
class DiffExplorerPanel;
class MergeEditor;

class GitPlugin : public QObject,
                  public WorkbenchPluginBase,
                  public IViewContributor,
                  public IAgentToolPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IViewContributor IAgentToolPlugin)

public:
    explicit GitPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    // IViewContributor
    QWidget *createView(const QString &viewId, QWidget *parent) override;

    // IAgentToolPlugin
    QString toolPluginName() const override { return QStringLiteral("Git"); }
    QStringList contexts() const override { return {QStringLiteral("git")}; }
    std::vector<std::unique_ptr<ITool>> createTools() override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    void wireCommitMessageGeneration();
    void wireConflictResolution();

    GitService        *m_git         = nullptr;
    GitPanel          *m_gitPanel    = nullptr;
    DiffExplorerPanel *m_diffExplorer = nullptr;
    MergeEditor       *m_mergeEditor  = nullptr;
};
