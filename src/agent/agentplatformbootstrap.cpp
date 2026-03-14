#include "agentplatformbootstrap.h"

#include "agentcontroller.h"
#include "agentorchestrator.h"
#include "agentproviderregistry.h"
#include "agentrequestrouter.h"
#include "braincontextbuilder.h"
#include "chatsessionservice.h"
#include "contextbuilder.h"
#include "iagentplugin.h"
#include "iagentsettingspageprovider.h"
#include "ichatsessionimporter.h"
#include "iproviderauthintegration.h"
#include "itool.h"
#include "memorysuggestionengine.h"
#include "projectbrainservice.h"
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
#include "tools/debugtools.h"
#include "tools/screenshottool.h"
#include "tools/introspecttool.h"
#include "tools/httptool.h"
#include "tools/luatool.h"
#include "tools/luaagenttoolstore.h"
#include "tools/codegraphtool.h"
#include "tools/buildtools.h"
#include "tools/navigationtools.h"
#include "tools/formatcodetool.h"
#include "tools/refactortool.h"
#include "tools/gitopstool.h"
#include "tools/askusertool.h"
#include "tools/editorcontexttool.h"
#include "tools/changeimpacttool.h"
#include "tools/scratchpadtool.h"
#include "tools/terminalsessiontools.h"
#include "tools/pythontools.h"
#include "tools/packagemanagertools.h"
#include "tools/subagenttool.h"
#include "tools/lsptools.h"
#include "tools/projectinfotool.h"
#include "tools/filemanagementtools.h"
#include "tools/clipboardtool.h"
#include "tools/difftool.h"
#include "tools/databasetool.h"
#include "tools/systemtools.h"
#include "tools/notebooktools.h"
#include "tools/githubmcptools.h"
#include "tools/idecommandtool.h"
#include "tools/sandboxtools.h"
#include "tools/devtools.h"
#include "tools/treesitterquerytool.h"
#include "tools/dashboardtools.h"
#include "ui/agentuibus.h"
#include "diagnosticsnotifier.h"
#include "terminalsessionmanager.h"
#include "workspacecontextdetector.h"
#include "../core/qtprocess.h"
#include "../pluginmanager.h"
#include "../serviceregistry.h"

#include <QStandardPaths>

#include <memory>

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
    m_sessionManager = new TerminalSessionManager({}, this);
    m_agentController = new AgentController(m_orchestrator, m_toolRegistry, m_contextBuilder, this);
    m_agentController->setSessionStore(m_sessionStore);

    // ── New platform services ─────────────────────────────────────────────
    m_providerRegistry = new AgentProviderRegistry(this);
    m_requestRouter    = new AgentRequestRouter(m_providerRegistry, this);
    m_chatSessionService = new ChatSessionService(m_sessionStore, this);
    m_toolApprovalService = new ToolApprovalService(this);

    // Wire the approval service into the controller
    m_agentController->setToolApprovalService(m_toolApprovalService);

    // Project brain (persistent workspace knowledge)
    m_brainService = new ProjectBrainService(this);
    m_brainBuilder = new BrainContextBuilder(m_brainService, this);
    m_memorySuggestionEngine = new MemorySuggestionEngine(m_brainService, this);
    m_agentController->setBrainContextBuilder(m_brainBuilder);

    // Agent UI event bus (dashboard protocol)
    m_uiBus = new AgentUIBus(this);

    m_contextBuilder->setFileSystem(m_fileSystem);
    m_contextBuilder->setOpenFilesGetter(m_callbacks.openFilesGetter);
    m_contextBuilder->setGitStatusGetter(m_callbacks.gitStatusGetter);
    m_contextBuilder->setTerminalOutputGetter(m_callbacks.terminalOutputGetter);
    m_contextBuilder->setDiagnosticsGetter(m_callbacks.diagnosticsGetter);

    // Adapt per-file gitDiffGetter to no-arg getter (full working tree diff)
    if (m_callbacks.gitDiffGetter) {
        m_contextBuilder->setGitDiffGetter([cb = m_callbacks.gitDiffGetter]() -> QString {
            return cb(QString()); // empty path = full diff
        });
    }

    if (m_services) {
        m_services->registerService(QStringLiteral("toolRegistry"), m_toolRegistry);
        m_services->registerService(QStringLiteral("contextBuilder"), m_contextBuilder);
        m_services->registerService(QStringLiteral("agentController"), m_agentController);
        m_services->registerService(QStringLiteral("sessionStore"), m_sessionStore);
        m_services->registerService(QStringLiteral("agentProviderRegistry"), m_providerRegistry);
        m_services->registerService(QStringLiteral("agentRequestRouter"), m_requestRouter);
        m_services->registerService(QStringLiteral("chatSessionService"), m_chatSessionService);
        m_services->registerService(QStringLiteral("toolApprovalService"), m_toolApprovalService);
        m_services->registerService(QStringLiteral("projectBrainService"), m_brainService);
        m_services->registerService(QStringLiteral("agentUIBus"), m_uiBus);
    }
}

void AgentPlatformBootstrap::registerCoreTools(const QString &workspaceRoot)
{
    if (!m_toolRegistry) {
        return;
    }

    m_toolRegistry->registerTool(std::make_unique<ReadFileTool>(m_fileSystem));
    m_toolRegistry->registerTool(std::make_unique<ListFilesTool>(m_fileSystem));
    m_toolRegistry->registerTool(std::make_unique<WriteFileTool>(m_fileSystem));
    m_toolRegistry->registerTool(std::make_unique<ApplyPatchTool>(m_fileSystem));
    m_toolRegistry->registerTool(std::make_unique<ReplaceStringTool>());
    m_toolRegistry->registerTool(std::make_unique<MultiReplaceStringTool>());
    m_toolRegistry->registerTool(std::make_unique<InsertEditIntoFileTool>());
    m_toolRegistry->registerTool(std::make_unique<SearchWorkspaceTool>(workspaceRoot));
    m_toolRegistry->registerTool(std::make_unique<SemanticSearchTool>(workspaceRoot));
    m_toolRegistry->registerTool(std::make_unique<FileSearchTool>(workspaceRoot));
    m_toolRegistry->registerTool(std::make_unique<CreateDirectoryTool>());
    m_toolRegistry->registerTool(std::make_unique<ManageTodoListTool>(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/agent_todo.json")));
    m_toolRegistry->registerTool(std::make_unique<ReadProjectStructureTool>(workspaceRoot));
    m_toolRegistry->registerTool(std::make_unique<RunCommandTool>(
        m_process.get(), workspaceRoot, m_sessionManager));

    // ── Terminal session tools (core, universal) ──────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<GetTerminalOutputTool>(m_sessionManager));
    m_toolRegistry->registerTool(std::make_unique<KillTerminalTool>(m_sessionManager));
    m_toolRegistry->registerTool(std::make_unique<AwaitTerminalTool>(m_sessionManager));
    if (m_callbacks.terminalSelectionGetter)
        m_toolRegistry->registerTool(std::make_unique<TerminalSelectionTool>(m_callbacks.terminalSelectionGetter));
    if (m_callbacks.terminalOutputGetter)
        m_toolRegistry->registerTool(std::make_unique<TerminalLastCommandTool>(m_callbacks.terminalOutputGetter));
    m_toolRegistry->registerTool(std::make_unique<GitStatusTool>(m_callbacks.gitStatusGetter));
    m_toolRegistry->registerTool(std::make_unique<GetChangedFilesTool>(m_callbacks.changedFilesGetter));
    m_toolRegistry->registerTool(std::make_unique<GitDiffTool>(m_callbacks.gitDiffGetter));
    m_toolRegistry->registerTool(std::make_unique<FetchWebpageTool>());
    m_toolRegistry->registerTool(std::make_unique<WebSearchTool>());
    m_toolRegistry->registerTool(std::make_unique<MemoryTool>(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/memories")));
    m_toolRegistry->registerTool(std::make_unique<GetErrorsTool>(m_callbacks.diagnosticsGetter));

    // ── Debug adapter tools ──────────────────────────────────────────────
    if (m_callbacks.debugBreakpointSetter) {
        m_toolRegistry->registerTool(std::make_unique<DebugSetBreakpointTool>(
            m_callbacks.debugBreakpointSetter, m_callbacks.debugBreakpointRemover));
    }
    if (m_callbacks.debugStackGetter) {
        m_toolRegistry->registerTool(std::make_unique<DebugGetStackTraceTool>(
            m_callbacks.debugStackGetter));
    }
    if (m_callbacks.debugVariablesGetter) {
        m_toolRegistry->registerTool(std::make_unique<DebugGetVariablesTool>(
            m_callbacks.debugVariablesGetter, m_callbacks.debugEvaluator));
    }
    if (m_callbacks.debugStepper) {
        m_toolRegistry->registerTool(std::make_unique<DebugStepTool>(
            m_callbacks.debugStepper));
    }

    // ── Screenshot tool ──────────────────────────────────────────────────
    if (m_callbacks.widgetGrabber) {
        m_toolRegistry->registerTool(std::make_unique<ScreenshotTool>(
            m_callbacks.widgetGrabber));
    }

    // ── Introspection tool ───────────────────────────────────────────────
    if (m_callbacks.introspectionHandler) {
        m_toolRegistry->registerTool(std::make_unique<IntrospectTool>(
            m_callbacks.introspectionHandler));
    }

    // ── HTTP request tool ────────────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<HttpRequestTool>());

    // ── Lua execution tool ───────────────────────────────────────────────
    if (m_callbacks.luaExecutor) {
        m_toolRegistry->registerTool(std::make_unique<LuaExecuteTool>(
            m_callbacks.luaExecutor));
    }

    // ── Agent tool store (save/list/run custom Lua tools) ────────────────
    m_toolRegistry->registerTool(std::make_unique<SaveLuaToolTool>());
    m_toolRegistry->registerTool(std::make_unique<ListLuaToolsTool>());
    if (m_callbacks.luaExecutor) {
        m_toolRegistry->registerTool(std::make_unique<RunLuaToolTool>(
            m_callbacks.luaExecutor));
    }

    // ── Code graph / intelligence tool ───────────────────────────────────
    if (m_callbacks.symbolSearchFn) {
        m_toolRegistry->registerTool(std::make_unique<CodeGraphTool>(
            m_callbacks.symbolSearchFn,
            m_callbacks.symbolsInFileFn,
            m_callbacks.findReferencesFn,
            m_callbacks.findDefinitionFn,
            m_callbacks.chunkSearchFn));
    }

    // ── Build & test tools ───────────────────────────────────────────────
    if (m_callbacks.buildProjectFn) {
        m_toolRegistry->registerTool(std::make_unique<BuildProjectTool>(
            m_callbacks.buildProjectFn));
    }
    if (m_callbacks.runTestsFn) {
        m_toolRegistry->registerTool(std::make_unique<RunTestsTool>(
            m_callbacks.runTestsFn));
    }
    if (m_callbacks.buildTargetsGetter) {
        m_toolRegistry->registerTool(std::make_unique<GetBuildTargetsTool>(
            m_callbacks.buildTargetsGetter));
    }
    if (m_callbacks.testFailureGetter) {
        m_toolRegistry->registerTool(std::make_unique<TestFailureTool>(
            m_callbacks.testFailureGetter));
    }

    // ── Navigation tools ─────────────────────────────────────────────────
    if (m_callbacks.fileOpener) {
        m_toolRegistry->registerTool(std::make_unique<OpenFileTool>(
            m_callbacks.fileOpener));
    }
    m_toolRegistry->registerTool(std::make_unique<SwitchHeaderSourceTool>(
        m_callbacks.headerSourceSwitcher));

    // ── Code formatting tool ──────────────────────────────────────────────
    if (m_callbacks.codeFormatter) {
        m_toolRegistry->registerTool(std::make_unique<FormatCodeTool>(
            m_callbacks.codeFormatter));
    }

    // ── Refactoring tool (LSP) ────────────────────────────────────────────
    if (m_callbacks.refactorer) {
        m_toolRegistry->registerTool(std::make_unique<RefactorTool>(
            m_callbacks.refactorer));
    }

    // ── Git operations tool ───────────────────────────────────────────────
    if (m_callbacks.gitExecutor) {
        m_toolRegistry->registerTool(std::make_unique<GitOpsTool>(
            m_callbacks.gitExecutor));
    }

    // ── Ask user tool ─────────────────────────────────────────────────────
    if (m_callbacks.userPrompter) {
        m_toolRegistry->registerTool(std::make_unique<AskUserTool>(
            m_callbacks.userPrompter));
    }

    // ── Editor context tool ───────────────────────────────────────────────
    if (m_callbacks.editorStateGetter) {
        m_toolRegistry->registerTool(std::make_unique<EditorContextTool>(
            m_callbacks.editorStateGetter));
    }

    // ── Change impact analysis tool ───────────────────────────────────────
    if (m_callbacks.changeImpactAnalyzer) {
        m_toolRegistry->registerTool(std::make_unique<ChangeImpactTool>(
            m_callbacks.changeImpactAnalyzer));
    }

    // ── Scratchpad (project-linked notes) ─────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<ScratchpadTool>(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/scratchpad")));

    // ── Sub-agent tool ────────────────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<SubagentTool>(
        m_orchestrator, m_toolRegistry, m_contextBuilder));

    // ── LSP rename & usages ───────────────────────────────────────────────
    if (m_callbacks.symbolRenamer) {
        m_toolRegistry->registerTool(std::make_unique<RenameSymbolTool>(
            m_callbacks.symbolRenamer));
    }
    if (m_callbacks.usageFinder) {
        m_toolRegistry->registerTool(std::make_unique<ListCodeUsagesTool>(
            m_callbacks.usageFinder));
    }

    // ── Project setup info ────────────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<GetProjectSetupInfoTool>(
        workspaceRoot));

    // ── File management tools (rename/delete/copy/watch) ──────────────────
    m_toolRegistry->registerTool(std::make_unique<DeleteFileTool>());
    m_toolRegistry->registerTool(std::make_unique<RenameFileTool>());
    m_toolRegistry->registerTool(std::make_unique<CopyFileTool>());
    m_toolRegistry->registerTool(std::make_unique<FileWatcherTool>());

    // ── Clipboard tool ────────────────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<ClipboardTool>());

    // ── Diff tool ─────────────────────────────────────────────────────────
    {
        auto diffTool = std::make_unique<DiffTool>(m_callbacks.diffViewer);
        diffTool->setWorkspaceRoot(workspaceRoot);
        m_toolRegistry->registerTool(std::move(diffTool));
    }

    // ── Database tool (SQLite) ────────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<DatabaseTool>());

    // ── System / process management tool ──────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<ProcessManagementTool>());

    // ── Network diagnostics tool ──────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<NetworkTool>());

    // ── Static analysis tool ──────────────────────────────────────────────
    if (m_callbacks.staticAnalyzer) {
        m_toolRegistry->registerTool(std::make_unique<StaticAnalysisTool>(
            m_callbacks.staticAnalyzer));
    }

    // ── IDE command execution tool ────────────────────────────────────
    if (m_callbacks.commandExecutor) {
        m_toolRegistry->registerTool(std::make_unique<RunIdeCommandTool>(
            m_callbacks.commandExecutor, m_callbacks.commandListGetter));
    }

    // ── Sandbox utility tools (JSON, time, regex, env vars, hash, config) ──
    m_toolRegistry->registerTool(std::make_unique<JsonParseFormatTool>());
    m_toolRegistry->registerTool(std::make_unique<CurrentTimeTool>());
    m_toolRegistry->registerTool(std::make_unique<RegexTestTool>());
    m_toolRegistry->registerTool(std::make_unique<EnvironmentVariablesTool>());
    m_toolRegistry->registerTool(std::make_unique<FileHashTool>());
    m_toolRegistry->registerTool(std::make_unique<WorkspaceConfigTool>(m_fileSystem));

    // ── Development workflow tools ────────────────────────────────────────
    m_toolRegistry->registerTool(std::make_unique<CompileAndRunTool>());
    m_toolRegistry->registerTool(std::make_unique<ArchiveTool>());
    m_toolRegistry->registerTool(std::make_unique<CreatePatchTool>());
    m_toolRegistry->registerTool(std::make_unique<ImageInfoTool>());

    // ── Tree-sitter AST parsing ───────────────────────────────────────────
    if (m_callbacks.treeSitterParser) {
        m_toolRegistry->registerTool(std::make_unique<TreeSitterParseTool>(
            m_callbacks.treeSitterParser));
    }

    // ── Tree-sitter rich AST query ────────────────────────────────────────
    if (m_callbacks.tsFileParser && m_callbacks.tsQueryRunner &&
        m_callbacks.tsSymbolExtractor && m_callbacks.tsNodeAtPosition) {
        m_toolRegistry->registerTool(std::make_unique<TreeSitterQueryTool>(
            m_callbacks.tsFileParser,
            m_callbacks.tsQueryRunner,
            m_callbacks.tsSymbolExtractor,
            m_callbacks.tsNodeAtPosition));
    }

    // ── LSP diagnostics real-time push ────────────────────────────────────
    m_diagnosticsNotifier = std::make_unique<DiagnosticsNotifier>(this);
    if (m_agentController) {
        m_agentController->setDiagnosticsNotifier(m_diagnosticsNotifier.get());
    }

    // ── Diagram generation (Mermaid/PlantUML) ─────────────────────────────
    if (m_callbacks.diagramRenderer) {
        m_toolRegistry->registerTool(std::make_unique<GenerateDiagramTool>(
            m_callbacks.diagramRenderer));
    }

    // ── Performance profiling ─────────────────────────────────────────────
    if (m_callbacks.profiler) {
        m_toolRegistry->registerTool(std::make_unique<PerformanceProfileTool>(
            m_callbacks.profiler));
    }

    // ── Symbol documentation (LSP hover) ──────────────────────────────────
    if (m_callbacks.symbolDocGetter) {
        m_toolRegistry->registerTool(std::make_unique<SymbolDocTool>(
            m_callbacks.symbolDocGetter));
    }

    // ── Code completion (LSP) ─────────────────────────────────────────────
    if (m_callbacks.completionGetter) {
        m_toolRegistry->registerTool(std::make_unique<CodeCompletionTool>(
            m_callbacks.completionGetter));
    }

    // ── Managed tools (language-specific, context-filtered) ────────────
    // These tools declare their contexts in ToolSpec::contexts.
    // ToolRegistry filters them based on detected workspace contexts.

    // Python tools (context: "python")
    m_toolRegistry->registerTool(std::make_unique<PythonEnvTool>(m_sessionManager));
    m_toolRegistry->registerTool(std::make_unique<InstallPythonPackagesTool>(m_sessionManager));
    m_toolRegistry->registerTool(std::make_unique<RunPythonTool>(m_sessionManager));

    // Node/Web tools (context: "web", "node")
    m_toolRegistry->registerTool(std::make_unique<PackageJsonInfoTool>());
    m_toolRegistry->registerTool(std::make_unique<NpmRunTool>(m_sessionManager));
    m_toolRegistry->registerTool(std::make_unique<InstallNodePackagesTool>(m_sessionManager));

    // Notebook / Jupyter tools (context: "notebook")
    {
        auto *nbMgr = new NotebookManager(this);
        m_toolRegistry->registerTool(std::make_unique<NotebookContextTool>(nbMgr));
        m_toolRegistry->registerTool(std::make_unique<ReadCellOutputTool>(nbMgr));
        m_toolRegistry->registerTool(std::make_unique<CreateNotebookTool>());
        m_toolRegistry->registerTool(std::make_unique<EditNotebookCellsTool>());
        m_toolRegistry->registerTool(std::make_unique<GetNotebookSummaryTool>());
    }

    // GitHub integration tools
    m_toolRegistry->registerTool(std::make_unique<GitHubIssuesTool>());
    m_toolRegistry->registerTool(std::make_unique<GitHubPRTool>());
    m_toolRegistry->registerTool(std::make_unique<GitHubCodeSearchTool>());
    m_toolRegistry->registerTool(std::make_unique<GitHubRepoInfoTool>());

    // ── Agent dashboard tools (live operational UI) ──────────────────────
    if (m_uiBus) {
        m_toolRegistry->registerTool(std::make_unique<CreateDashboardMissionTool>(m_uiBus));
        m_toolRegistry->registerTool(std::make_unique<UpdateDashboardStepTool>(m_uiBus));
        m_toolRegistry->registerTool(std::make_unique<UpdateDashboardMetricTool>(m_uiBus));
        m_toolRegistry->registerTool(std::make_unique<AddDashboardLogTool>(m_uiBus));
        m_toolRegistry->registerTool(std::make_unique<AddDashboardArtifactTool>(m_uiBus));
        m_toolRegistry->registerTool(std::make_unique<CompleteDashboardMissionTool>(m_uiBus));
    }

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

        // ── Discover tool plugins ───────────────────────────────────
        if (auto *toolPlugin = qobject_cast<IAgentToolPlugin *>(obj)) {
            auto tools = toolPlugin->createTools();
            for (auto &tool : tools) {
                if (m_toolRegistry)
                    m_toolRegistry->registerTool(std::move(tool));
            }
        }
    }

    // ── Register C ABI plugin providers ───────────────────────────────
    for (IAgentProvider *provider : pluginManager->cabiProviders()) {
        m_orchestrator->registerProvider(provider);
        if (m_providerRegistry)
            m_providerRegistry->registerProvider(provider);
    }
}

void AgentPlatformBootstrap::setWorkspaceRoot(const QString &root)
{
    if (m_contextBuilder) {
        m_contextBuilder->setWorkspaceRoot(root);
    }

    if (m_brainService) {
        m_brainService->load(root);
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

    // Update terminal session manager working directory
    if (m_sessionManager) {
        m_sessionManager->setWorkingDirectory(root);
    }

    // ── Detect workspace contexts and update tool filtering ────────
    const QSet<QString> contexts = WorkspaceContextDetector::detect(root);
    m_toolRegistry->setActiveContexts(contexts);

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

    auto *setupInfoTool = dynamic_cast<GetProjectSetupInfoTool *>(m_toolRegistry->tool(QStringLiteral("get_project_setup_info")));
    if (setupInfoTool) {
        setupInfoTool->setWorkspaceRoot(root);
    }

    // ── Set workspace root on new tools ───────────────────────────────────
    auto *delTool = dynamic_cast<DeleteFileTool *>(m_toolRegistry->tool(QStringLiteral("delete_file")));
    if (delTool) delTool->setWorkspaceRoot(root);

    auto *renameTool = dynamic_cast<RenameFileTool *>(m_toolRegistry->tool(QStringLiteral("rename_file")));
    if (renameTool) renameTool->setWorkspaceRoot(root);

    auto *copyTool = dynamic_cast<CopyFileTool *>(m_toolRegistry->tool(QStringLiteral("copy_file")));
    if (copyTool) copyTool->setWorkspaceRoot(root);

    auto *watchTool = dynamic_cast<FileWatcherTool *>(m_toolRegistry->tool(QStringLiteral("file_watcher")));
    if (watchTool) watchTool->setWorkspaceRoot(root);

    auto *diffTool = dynamic_cast<DiffTool *>(m_toolRegistry->tool(QStringLiteral("diff")));
    if (diffTool) diffTool->setWorkspaceRoot(root);

    auto *dbTool = dynamic_cast<DatabaseTool *>(m_toolRegistry->tool(QStringLiteral("database_query")));
    if (dbTool) dbTool->setWorkspaceRoot(root);

    // ── Set workspace root on sandbox & dev tools ─────────────────────────
    auto *hashTool = dynamic_cast<FileHashTool *>(m_toolRegistry->tool(QStringLiteral("file_content_hash")));
    if (hashTool) hashTool->setWorkspaceRoot(root);

    auto *wsConfigTool = dynamic_cast<WorkspaceConfigTool *>(m_toolRegistry->tool(QStringLiteral("workspace_config")));
    if (wsConfigTool) wsConfigTool->setWorkspaceRoot(root);

    auto *compileRunTool = dynamic_cast<CompileAndRunTool *>(m_toolRegistry->tool(QStringLiteral("compile_and_run")));
    if (compileRunTool) compileRunTool->setWorkspaceRoot(root);

    auto *archiveTool = dynamic_cast<ArchiveTool *>(m_toolRegistry->tool(QStringLiteral("archive")));
    if (archiveTool) archiveTool->setWorkspaceRoot(root);

    auto *patchTool = dynamic_cast<CreatePatchTool *>(m_toolRegistry->tool(QStringLiteral("create_patch_file")));
    if (patchTool) patchTool->setWorkspaceRoot(root);

    auto *imgTool = dynamic_cast<ImageInfoTool *>(m_toolRegistry->tool(QStringLiteral("image_info")));
    if (imgTool) imgTool->setWorkspaceRoot(root);

    auto *diagramTool = dynamic_cast<GenerateDiagramTool *>(m_toolRegistry->tool(QStringLiteral("generate_diagram")));
    if (diagramTool) diagramTool->setWorkspaceRoot(root);
}

DiagnosticsNotifier *AgentPlatformBootstrap::diagnosticsNotifier() const
{
    return m_diagnosticsNotifier.get();
}