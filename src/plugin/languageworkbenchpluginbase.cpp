#include "languageworkbenchpluginbase.h"

#include "lsp/lspclient.h"
#include "lsp/processlanguageserver.h"
#include "sdk/ilspservice.h"

namespace {

constexpr const char *kProjectDockId = "ProjectDock";
constexpr const char *kProblemsDockId = "ProblemsDock";
constexpr const char *kSearchDockId = "SearchDock";
constexpr const char *kTerminalDockId = "TerminalDock";
constexpr const char *kGitDockId = "GitDock";

}

QString LanguageWorkbenchPluginBase::languageWorkspaceRoot() const
{
    return workspaceRoot();
}

void LanguageWorkbenchPluginBase::showProjectTree() const
{
    showPanel(QString::fromLatin1(kProjectDockId));
}

void LanguageWorkbenchPluginBase::showProblemsPanel() const
{
    showPanel(QString::fromLatin1(kProblemsDockId));
}

void LanguageWorkbenchPluginBase::showSearchPanel() const
{
    showPanel(QString::fromLatin1(kSearchDockId));
}

void LanguageWorkbenchPluginBase::showTerminalPanel() const
{
    showPanel(QString::fromLatin1(kTerminalDockId));
}

void LanguageWorkbenchPluginBase::showGitPanel() const
{
    showPanel(QString::fromLatin1(kGitDockId));
}

void LanguageWorkbenchPluginBase::showStandardLanguagePanels() const
{
    showProjectTree();
    showProblemsPanel();
}

void LanguageWorkbenchPluginBase::registerLanguageServiceObjects(QObject *client,
                                                                 QObject *serviceObject) const
{
    if (client)
        registerService(QStringLiteral("lspClient"), client);
    if (serviceObject)
        registerService(QStringLiteral("lspService"), serviceObject);
}

void LanguageWorkbenchPluginBase::attachLanguageServerToWorkbench(ProcessLanguageServer *server) const
{
    if (!server)
        return;

    registerLanguageServiceObjects(server->client(), server->service());

    QObject::connect(server, &ProcessLanguageServer::statusMessage,
                     [this](const QString &message, int timeoutMs) {
                         showStatusMessage(message, timeoutMs);
                     });
}

std::unique_ptr<ProcessLanguageServer> LanguageWorkbenchPluginBase::createProcessLanguageServer(
    const QString &program,
    const QString &displayName,
    const QStringList &baseArguments) const
{
    return std::make_unique<ProcessLanguageServer>(
        ProcessLanguageServer::Config{ program, displayName, baseArguments });
}

void LanguageWorkbenchPluginBase::startProcessLanguageServer(ProcessLanguageServer *server,
                                                             const QStringList &extraArgs) const
{
    if (!server)
        return;

    const QString root = languageWorkspaceRoot().isEmpty()
        ? server->workspaceRoot()
        : languageWorkspaceRoot();
    server->start(root, extraArgs);
}

void LanguageWorkbenchPluginBase::stopProcessLanguageServer(ProcessLanguageServer *server) const
{
    if (server)
        server->stop();
}
