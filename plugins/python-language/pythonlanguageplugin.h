#pragma once

#include "plugin/languageworkbenchpluginbase.h"

#include <memory>

class ProcessLanguageServer;

class PythonLanguagePlugin : public QObject, public LanguageWorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    explicit PythonLanguagePlugin(QObject *parent = nullptr);
    ~PythonLanguagePlugin() override;

    PluginInfo info() const override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    void registerPythonCommands();
    void installPythonMenu();

    std::unique_ptr<ProcessLanguageServer> m_server;
};
