#include "agentplatformbootstrap.h"

#include "agentcontroller.h"
#include "agentorchestrator.h"
#include "agentproviderregistry.h"
#include "agentrequestrouter.h"
#include "chatsessionservice.h"
#include "contextbuilder.h"
#include "iagentplugin.h"
#include "iagentsettingspageprovider.h"
#include "ichatsessionimporter.h"
#include "iproviderauthintegration.h"
#include "itool.h"
#include "sessionstore.h"
#include "toolapprovalservice.h"
#include "tools/advancedtools.h"
#include "tools/applypatchtool.h"
#include "tools/filesystemtools.h"
#include "tools/idetools.h"
#include "tools/memorytool.h"
#include "tools/moretools.h"
#include "tools/runcommandtool.h"
#include "tools/searchworkspacetool.h"
#include "tools/semanticsearchtool.h"
#include "tools/websearchtool.h"
#include "../core/qtprocess.h"
#include "../pluginmanager.h"
#include "../serviceregistry.h"

#include <QStandardPaths>

AgentPlatformBootstrap::AgentPlatformBootstrap(AgentOrchestrator *orchestrator,
                                               ServiceRegistry *services,
                                               IFileSystem *fileSystem,
                                               QObject *parent)
    : QObject(parent)
    , m_orchestrator(orchestrator)
    , m_services(services)
    , m_fileSystem(fileSystem)
    , m_process(std::make_unique<QtProcess>())
{
}

void AgentPlatformBootstrap::initialize(const Callbacks &callbacks)
{
    m_callbacks = callbacks;

    m_toolRegistry = new ToolRegistry(this);
    m_contextBuilder = new ContextBuilder(this);
    m_sessionStore = new SessionStore(this);
    m_agentController = new AgentController(m_orchestrator, m_toolRegistry, m_contextBuilder, this);
    m_agentController->setSessionStore(m_sessionStore);

    // ── New platform services ─────────────────────────────────────────────
    m_providerRegistry = new AgentProviderRegistry(this);
    m_requestRouter    = new AgentRequestRouter(m_providerRegistry, this);
    m_chatSessionService = new ChatSessionService(m_sessionStore, this);
    m_toolApprovalService = new ToolApprovalService(this);

    // Wire the approval service into the controller
    m_agentController->setToolApprovalService(m_toolApprovalService);

    m_contextBuilder->setFileSystem(m_fileSystem);
    m_contextBuilder->setOpenFilesGetter(m_callbacks.openFilesGetter);
    m_contextBuilder->setGitStatusGetter(m_callbacks.gitStatusGetter);
    m_contextBuilder->setTerminalOutputGetter(m_callbacks.terminalOutputGetter);

    if (m_services) {
        m_services->registerService(QStringLiteral("toolRegistry"), m_toolRegistry);
        m_services->registerService(QStringLiteral("contextBuilder"), m_contextBuilder);
        m_services->registerService(QStringLiteral("agentController"), m_agentController);
        m_services->registerService(QStringLiteral("sessionStore"), m_sessionStore);
        m_services->registerService(QStringLiteral("agentProviderRegistry"), m_providerRegistry);
        m_services->registerService(QStringLiteral("agentRequestRouter"), m_requestRouter);
        m_services->registerService(QStringLiteral("chatSessionService"), m_chatSessionService);
        m_services->registerService(QStringLiteral("toolApprovalService"), m_toolApprovalService);
    }
}

void AgentPlatformBootstrap::registerCoreTools(const QString &workspaceRoot)
{
    if (!m_toolRegistry) {
        return;
    }

    m_toolRegistry->registerTool(new ReadFileTool(m_fileSystem));
    m_toolRegistry->registerTool(new ListFilesTool(m_fileSystem));
    m_toolRegistry->registerTool(new WriteFileTool(m_fileSystem));
    m_toolRegistry->registerTool(new ApplyPatchTool(m_fileSystem));
    m_toolRegistry->registerTool(new ReplaceStringTool());
    m_toolRegistry->registerTool(new MultiReplaceStringTool());
    m_toolRegistry->registerTool(new InsertEditIntoFileTool());
    m_toolRegistry->registerTool(new SearchWorkspaceTool(workspaceRoot));
    m_toolRegistry->registerTool(new SemanticSearchTool(workspaceRoot));
    m_toolRegistry->registerTool(new FileSearchTool(workspaceRoot));
    m_toolRegistry->registerTool(new CreateDirectoryTool());
    m_toolRegistry->registerTool(new ManageTodoListTool(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/agent_todo.json")));
    m_toolRegistry->registerTool(new ReadProjectStructureTool(workspaceRoot));
    m_toolRegistry->registerTool(new RunCommandTool(m_process.get(), workspaceRoot));
    m_toolRegistry->registerTool(new GitStatusTool(m_callbacks.gitStatusGetter));
    m_toolRegistry->registerTool(new GetChangedFilesTool(m_callbacks.changedFilesGetter));
    m_toolRegistry->registerTool(new GitDiffTool(m_callbacks.gitDiffGetter));
    m_toolRegistry->registerTool(new FetchWebpageTool());
    m_toolRegistry->registerTool(new WebSearchTool());
    m_toolRegistry->registerTool(new MemoryTool(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/memories")));
    m_toolRegistry->registerTool(new GetErrorsTool(m_callbacks.diagnosticsGetter));

    setWorkspaceRoot(workspaceRoot);
}

void AgentPlatformBootstrap::registerPluginProviders(PluginManager *pluginManager)
{
    if (!pluginManager || !m_orchestrator) {
        return;
    }

    for (QObject *obj : pluginManager->pluginObjects()) {
        auto *agentPlugin = qobject_cast<IAgentPlugin *>(obj);
        if (!agentPlugin) {
            continue;
        }

        const auto providers = agentPlugin->createProviders(m_orchestrator);
        for (IAgentProvider *provider : providers) {
            m_orchestrator->registerProvider(provider);
            if (m_providerRegistry)
                m_providerRegistry->registerProvider(provider);
        }

        // ── Discover plugin extension interfaces ──────────────────────
        if (auto *importer = qobject_cast<IChatSessionImporter *>(obj))
            m_sessionImporters.append(importer);

        if (auto *auth = qobject_cast<IProviderAuthIntegration *>(obj))
            m_authIntegrations.append(auth);

        if (auto *settings = qobject_cast<IAgentSettingsPageProvider *>(obj))
            m_settingsPages.append(settings);
    }
}

void AgentPlatformBootstrap::setWorkspaceRoot(const QString &root)
{
    if (m_contextBuilder) {
        m_contextBuilder->setWorkspaceRoot(root);
    }

    if (!m_toolRegistry) {
        return;
    }

    auto *searchTool = dynamic_cast<SearchWorkspaceTool *>(m_toolRegistry->tool(QStringLiteral("grep_search")));
    if (searchTool) {
        searchTool->setWorkspaceRoot(root);
    }

    auto *semSearchTool = dynamic_cast<SemanticSearchTool *>(m_toolRegistry->tool(QStringLiteral("semantic_search")));
    if (semSearchTool) {
        semSearchTool->setWorkspaceRoot(root);
    }

    auto *fileSearchTool = dynamic_cast<FileSearchTool *>(m_toolRegistry->tool(QStringLiteral("file_search")));
    if (fileSearchTool) {
        fileSearchTool->setWorkspaceRoot(root);
    }

    auto *runCommandTool = dynamic_cast<RunCommandTool *>(m_toolRegistry->tool(QStringLiteral("run_in_terminal")));
    if (runCommandTool) {
        runCommandTool->setWorkingDirectory(root);
    }

    auto *readTool = dynamic_cast<ReadFileTool *>(m_toolRegistry->tool(QStringLiteral("read_file")));
    if (readTool) {
        readTool->setWorkspaceRoot(root);
    }

    auto *listTool = dynamic_cast<ListFilesTool *>(m_toolRegistry->tool(QStringLiteral("list_dir")));
    if (listTool) {
        listTool->setWorkspaceRoot(root);
    }

    auto *writeTool = dynamic_cast<WriteFileTool *>(m_toolRegistry->tool(QStringLiteral("create_file")));
    if (writeTool) {
        writeTool->setWorkspaceRoot(root);
    }

    auto *projectTool = dynamic_cast<ReadProjectStructureTool *>(m_toolRegistry->tool(QStringLiteral("read_project_structure")));
    if (projectTool) {
        projectTool->setWorkspaceRoot(root);
    }
}