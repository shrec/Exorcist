
#pragma once

#include <QObject>

#include <utility>

#include "embeddedcommandresolver.h"
#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

class EmbeddedToolsPanel;

class EmbeddedToolsPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit EmbeddedToolsPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void registerCommands();
    void installMenusAndToolBar();
    void updatePanelHints();
    EmbeddedCommandResolution inferredResolution() const;
    EmbeddedCommandResolution commandOverrides() const;
    void persistCommandOverrides(const QString &flashCommand,
                                 const QString &monitorCommand,
                                 const QString &debugCommand) const;
    void clearCommandOverrides() const;
    QString workspaceSettingsKey() const;
    EmbeddedCommandResolution currentResolution() const;

    EmbeddedToolsPanel *m_panel = nullptr;
    EmbeddedCommandResolver m_commandResolver;
};