#pragma once

#include "plugin/workbenchpluginbase.h"

#include <memory>
#include <QStringList>

class LspClient;
class ProcessLanguageServer;

/// Shared base for language and tooling plugins that need the standard
/// workbench surface: project tree, problems, terminal, search, git, and
/// common language-service registration.
class LanguageWorkbenchPluginBase : public WorkbenchPluginBase
{
protected:
    LanguageWorkbenchPluginBase() = default;
    ~LanguageWorkbenchPluginBase() override = default;

    QString languageWorkspaceRoot() const;

    void showProjectTree() const;
    void showProblemsPanel() const;
    void showSearchPanel() const;
    void showTerminalPanel() const;
    void showGitPanel() const;
    void showStandardLanguagePanels() const;

    void registerLanguageServiceObjects(QObject *client, QObject *serviceObject) const;
    void attachLanguageServerToWorkbench(ProcessLanguageServer *server) const;
    std::unique_ptr<ProcessLanguageServer> createProcessLanguageServer(
        const QString &program,
        const QString &displayName,
        const QStringList &baseArguments = {}) const;
    void startProcessLanguageServer(ProcessLanguageServer *server,
                                    const QStringList &extraArgs = {}) const;
    void stopProcessLanguageServer(ProcessLanguageServer *server) const;

    // ── Standard LSP Command Registration ─────────────────────────────────
    // Registers: {prefix}.goToDefinition, .goToDeclaration, .findReferences,
    //            .renameSymbol, .formatDocument, .gotoWorkspaceSymbol,
    //            .restartLanguageServer, .openProblems, .openSearch,
    //            .openTerminal, .openGit
    void registerStandardLspCommands(const QString &prefix,
                                     LspClient *client,
                                     ProcessLanguageServer *server,
                                     QObject *owner);

    // Standard language menu with all LSP commands.
    void installStandardLspMenu(const QString &menuId,
                                const QString &menuTitle,
                                const QString &prefix,
                                QObject *owner);

    // Standard language toolbar.
    void installStandardLspToolBar(const QString &toolBarId,
                                   const QString &toolBarTitle,
                                   const QString &prefix,
                                   QObject *owner);

    // ── Status Bar ────────────────────────────────────────────────────────
    void installLanguageStatusItem(const QString &itemId,
                                   const QString &initialText,
                                   const QString &tooltip = {});
    void updateLanguageStatus(const QString &itemId,
                              const QString &text,
                              const QString &tooltip = {});
    void removeLanguageStatusItem(const QString &itemId);

    // ── Auto-restart on crash ─────────────────────────────────────────────
    // Sets up crash → auto-restart with status notifications.
    // Returns restart count (reset on successful serverReady).
    void setupServerAutoRestart(ProcessLanguageServer *server,
                                const QString &statusItemId,
                                const QString &displayName,
                                int maxRestarts = 3);
};
