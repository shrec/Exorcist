#pragma once

#include "plugin/workbenchpluginbase.h"

class QWidget;
class QtHelpDock;

class QtToolsPlugin : public QObject, public WorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    explicit QtToolsPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;

private:
    bool initializePlugin() override;
    void registerCommands();
    void registerFileExtensionEditors();
    void registerQtHelpDock();
    bool launchTool(const QString &program, const QStringList &arguments = {});

    QtHelpDock *m_qtHelpDock = nullptr;
};
