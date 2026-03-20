#include "workbenchcommandbootstrap.h"

#include "sdk/hostservices.h"

WorkbenchCommandBootstrap::WorkbenchCommandBootstrap(QObject *parent)
    : QObject(parent)
{
}

void WorkbenchCommandBootstrap::initialize(CommandServiceImpl *commands,
                                           const Callbacks &callbacks)
{
    if (!commands)
        return;

    commands->registerCommand(QStringLiteral("workbench.action.newTab"), tr("New Tab"),
                              callbacks.newTab);
    commands->registerCommand(QStringLiteral("workbench.action.openFile"), tr("Open File..."),
                              callbacks.openFile);
    commands->registerCommand(QStringLiteral("workbench.action.openFolder"), tr("Open Folder..."),
                              callbacks.openFolder);
    commands->registerCommand(QStringLiteral("workbench.action.save"), tr("Save"),
                              callbacks.save);
    commands->registerCommand(QStringLiteral("workbench.action.goToFile"), tr("Go to File..."),
                              callbacks.goToFile);
    commands->registerCommand(QStringLiteral("workbench.action.findReplace"), tr("Find / Replace"),
                              callbacks.findReplace);
    commands->registerCommand(QStringLiteral("workbench.action.toggleProject"), tr("Toggle Project Panel"),
                              callbacks.toggleProject);
    commands->registerCommand(QStringLiteral("workbench.action.toggleSearch"), tr("Toggle Search Panel"),
                              callbacks.toggleSearch);
    commands->registerCommand(QStringLiteral("workbench.action.toggleTerminal"), tr("Toggle Terminal Panel"),
                              callbacks.toggleTerminal);
    commands->registerCommand(QStringLiteral("workbench.action.toggleAI"), tr("Toggle AI Panel"),
                              callbacks.toggleAi);
    commands->registerCommand(QStringLiteral("workbench.action.goToSymbol"), tr("Go to Symbol..."),
                              callbacks.goToSymbol);
    commands->registerCommand(QStringLiteral("workbench.action.quit"), tr("Quit"),
                              callbacks.quit);
    commands->registerCommand(QStringLiteral("workbench.action.commandPalette"), tr("Command Palette"),
                              callbacks.commandPalette);
}
