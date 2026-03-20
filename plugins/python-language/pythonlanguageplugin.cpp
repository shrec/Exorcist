#include "pythonlanguageplugin.h"

#include "lsp/processlanguageserver.h"

PythonLanguagePlugin::PythonLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

PythonLanguagePlugin::~PythonLanguagePlugin() = default;

PluginInfo PythonLanguagePlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.python-language"),
        QStringLiteral("Python Language Support"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Process-based Python LSP integration for .py files."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool PythonLanguagePlugin::initializePlugin()
{
    m_server = createProcessLanguageServer(
        QStringLiteral("pylsp"),
        tr("Python language server"));
    attachLanguageServerToWorkbench(m_server.get());
    startProcessLanguageServer(m_server.get());
    return true;
}

void PythonLanguagePlugin::shutdownPlugin()
{
    stopProcessLanguageServer(m_server.get());
}
