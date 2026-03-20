#include "qttoolsplugin.h"

#include "sdk/icommandservice.h"

#include <QProcess>

QtToolsPlugin::QtToolsPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo QtToolsPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.qt-tools"),
        QStringLiteral("Qt Tools"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Launches Qt Designer, Linguist, and Assistant for Qt workspaces."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool QtToolsPlugin::initializePlugin()
{
    registerCommands();

    ensureMenu(QStringLiteral("qt.tools"), tr("&Qt"));
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Designer"),
                   QStringLiteral("qt.openDesigner"), this);
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Linguist"),
                   QStringLiteral("qt.openLinguist"), this);
    addMenuCommand(QStringLiteral("qt.tools"), tr("Open Qt &Assistant"),
                   QStringLiteral("qt.openAssistant"), this);

    createToolBar(QStringLiteral("qt-tools"), tr("Qt"));
    addToolBarCommands(QStringLiteral("qt-tools"), {
        {tr("Designer"), QStringLiteral("qt.openDesigner")},
        {tr("Linguist"), QStringLiteral("qt.openLinguist")},
        {tr("Assistant"), QStringLiteral("qt.openAssistant")},
    }, this);
    return true;
}

void QtToolsPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("qt.openDesigner"),
                              tr("Qt: Open Designer"),
                              [this]() { launchTool(QStringLiteral("designer")); });
    cmds->registerCommand(QStringLiteral("qt.openLinguist"),
                              tr("Qt: Open Linguist"),
                              [this]() { launchTool(QStringLiteral("linguist")); });
    cmds->registerCommand(QStringLiteral("qt.openAssistant"),
                              tr("Qt: Open Assistant"),
                              [this]() { launchTool(QStringLiteral("assistant")); });
}

bool QtToolsPlugin::launchTool(const QString &program, const QStringList &arguments)
{
    const QString workingDir = workspaceRoot();
    const bool started = QProcess::startDetached(program, arguments, workingDir);
    if (!started)
        showWarning(tr("Unable to launch %1. Ensure the Qt tool is installed and on PATH.").arg(program));
    return started;
}