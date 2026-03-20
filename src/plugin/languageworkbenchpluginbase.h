#pragma once

#include "plugin/workbenchpluginbase.h"

#include <memory>
#include <QStringList>

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
};
