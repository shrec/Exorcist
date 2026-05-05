#pragma once

#include <QObject>

#include "plugin/workbenchpluginbase.h"

class WelcomeWidget;

/// StartPagePlugin — owns the WelcomeWidget shown when no workspace is open.
///
/// Implements CLAUDE.md rule L3: the welcome / start page is a plugin, not a
/// MainWindow concern.  Lifecycle:
///   - initializePlugin: create WelcomeWidget, register as service
///                       "welcomeWidget", wire signals to commands.
///   - onWorkspaceClosed: trigger a refresh of the recent folders list.
///   - shutdownPlugin: WorkbenchPluginBase auto-cleans tracked artifacts.
///
/// MainWindow queries the "welcomeWidget" service in deferredInit and
/// places the returned widget into its central stack at index 0.  A future
/// pass will replace the central-stack model entirely with plugin-owned
/// center docks.
class StartPagePlugin : public QObject, public WorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    explicit StartPagePlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

    /// Workspace lifecycle hooks.  When workspace opens we don't hide the
    /// widget here (MainWindow's central stack switches to the editor page);
    /// when it closes we refresh the recent list so the just-closed folder
    /// shows at the top.
    void onWorkspaceClosed() override;

private:
    bool initializePlugin() override;

    WelcomeWidget *m_welcome = nullptr;
};
