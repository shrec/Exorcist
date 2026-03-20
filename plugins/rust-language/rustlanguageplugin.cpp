#include "rustlanguageplugin.h"

#include "lsp/processlanguageserver.h"

RustLanguagePlugin::RustLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

RustLanguagePlugin::~RustLanguagePlugin() = default;

PluginInfo RustLanguagePlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.rust-language"),
        QStringLiteral("Rust Language Support"),
        QStringLiteral("1.0.0"),
        QStringLiteral("rust-analyzer integration for Cargo and Rust workspaces."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool RustLanguagePlugin::initializePlugin()
{
    m_server = createProcessLanguageServer(
        QStringLiteral("rust-analyzer"),
        tr("Rust language server"));
    attachLanguageServerToWorkbench(m_server.get());
    startProcessLanguageServer(m_server.get());
    return true;
}

void RustLanguagePlugin::shutdownPlugin()
{
    stopProcessLanguageServer(m_server.get());
}
