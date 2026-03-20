#pragma once

#include "plugin/languageworkbenchpluginbase.h"

#include <memory>

class ProcessLanguageServer;

class RustLanguagePlugin : public QObject, public LanguageWorkbenchPluginBase
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    explicit RustLanguagePlugin(QObject *parent = nullptr);
    ~RustLanguagePlugin() override;

    PluginInfo info() const override;

private:
    bool initializePlugin() override;
    void shutdownPlugin() override;

    std::unique_ptr<ProcessLanguageServer> m_server;
};