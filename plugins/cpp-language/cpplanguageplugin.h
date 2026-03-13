#pragma once

#include "plugininterface.h"

#include <QString>

class ClangdManager;
class LspClient;
class ILspService;

class CppLanguagePlugin : public QObject, public IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin)

public:
    explicit CppLanguagePlugin(QObject *parent = nullptr);

    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override;

private:
    IHostServices   *m_host       = nullptr;
    ClangdManager   *m_clangd     = nullptr;
    LspClient       *m_lspClient  = nullptr;
    ILspService     *m_lspService = nullptr;
};
