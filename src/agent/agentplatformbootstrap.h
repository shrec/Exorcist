#ifndef D5B507CA_8058_4248_8C39_B793CE7AFB48
#define D5B507CA_8058_4248_8C39_B793CE7AFB48

#include <functional>
#include <memory>

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include "../aiinterface.h"
#include "../core/iprocess.h"
#include "tools/luatool.h"
#include "tools/buildtools.h"
#include "tools/formatcodetool.h"
#include "tools/refactortool.h"
#include "tools/gitopstool.h"
#include "tools/askusertool.h"
#include "tools/editorcontexttool.h"
#include "tools/changeimpacttool.h"
#include "tools/lsptools.h"
#include "tools/difftool.h"
#include "tools/staticanalysistool.h"
#include "tools/devtools.h"
#include "tools/treesitterquerytool.h"

class DiagnosticsNotifier;
class IFileSystem;
class PluginManager;
class ServiceRegistry;
class AgentOrchestrator;
class AgentController;
class AgentProviderRegistry;
class AgentRequestRouter;
class BrainContextBuilder;
class ChatSessionService;
class ContextBuilder;
class IAgentSettingsPageProvider;
class IChatSessionImporter;
class IProviderAuthIntegration;
class MemorySuggestionEngine;
class ProjectBrainService;
class SessionStore;
class TerminalSessionManager;
class ToolApprovalService;
class ToolRegistry;

class AgentUIBus;

class AgentPlatformBootstrap : public QObject
{
    Q_OBJECT

public:
    struct Callbacks {
        std::function<QStringList()> openFilesGetter;
        std::function<QString()> gitStatusGetter;
        std::function<QString()> terminalOutputGetter;
        std::function<QList<AgentDiagnostic>()> diagnosticsGetter;
        std::function<QHash<QString, QString>()> changedFilesGetter;
        std::function<QString(const QString &)> gitDiffGetter;

        // Debug adapter callbacks
        std::function<QString(const QString &, int, const QString &)> debugBreakpointSetter;
        std::function<bool(const QString &, int)> debugBreakpointRemover;
        std::function<QString(int)> debugStackGetter;
        std::function<QString(int)> debugVariablesGetter;
        std::function<QString(const QString &, int)> debugEvaluator;
        std::function<bool(const QString &)> debugStepper;

        // Screenshot
        std::function<QPixmap(const QString &)> widgetGrabber;

        // Introspection
        std::function<QString(const QString &)> introspectionHandler;

        // Lua execution
        LuaExecuteTool::LuaExecutor luaExecutor;

        // Code graph / intelligence
        std::function<QString(const QString &, int)>              symbolSearchFn;
        std::function<QString(const QString &)>                   symbolsInFileFn;
        std::function<QString(const QString &, int, int)>         findReferencesFn;
        std::function<QString(const QString &, int, int)>         findDefinitionFn;
        std::function<QString(const QString &, int)>              chunkSearchFn;

        // Build & test
        BuildProjectTool::Builder       buildProjectFn;
        RunTestsTool::TestRunner         runTestsFn;
        std::function<QStringList()>     buildTargetsGetter;

        // Code formatting
        FormatCodeTool::CodeFormatter   codeFormatter;

        // Refactoring (LSP)
        RefactorTool::Refactorer        refactorer;

        // Git operations
        GitOpsTool::GitExecutor         gitExecutor;

        // User interaction
        AskUserTool::UserPrompter       userPrompter;

        // Editor context
        EditorContextTool::EditorStateGetter editorStateGetter;

        // Change impact analysis
        ChangeImpactTool::ImpactAnalyzer    changeImpactAnalyzer;

        // Navigation
        std::function<bool(const QString &, int, int)>           fileOpener;
        std::function<QString(const QString &)>                   headerSourceSwitcher;

        // LSP rename & usages
        RenameSymbolTool::SymbolRenamer     symbolRenamer;
        ListCodeUsagesTool::UsageFinder     usageFinder;

        // Diff visualization
        DiffTool::DiffViewer                diffViewer;

        // Static analysis
        StaticAnalysisTool::StaticAnalyzer  staticAnalyzer;

        // Terminal selection
        std::function<QString()>            terminalSelectionGetter;

        // Test failure cache
        std::function<RunTestsTool::TestResult()> testFailureGetter;

        // IDE command execution
        std::function<bool(const QString &)> commandExecutor;
        std::function<QStringList()>         commandListGetter;

        // Tree-sitter AST parsing
        TreeSitterParseTool::TreeSitterParser treeSitterParser;

        // Tree-sitter rich AST query
        TreeSitterQueryTool::FileParserFn      tsFileParser;
        TreeSitterQueryTool::QueryRunnerFn     tsQueryRunner;
        TreeSitterQueryTool::SymbolExtractorFn tsSymbolExtractor;
        TreeSitterQueryTool::NodeAtPositionFn  tsNodeAtPosition;

        // Diagram generation (Mermaid/PlantUML)
        GenerateDiagramTool::DiagramRenderer diagramRenderer;

        // Performance profiling
        PerformanceProfileTool::Profiler     profiler;

        // Symbol documentation (LSP hover)
        SymbolDocTool::DocGetter             symbolDocGetter;

        // Code completion (LSP)
        CodeCompletionTool::CompletionGetter completionGetter;
    };

    explicit AgentPlatformBootstrap(AgentOrchestrator *orchestrator,
                                    ServiceRegistry *services,
                                    IFileSystem *fileSystem,
                                    QObject *parent = nullptr);

    void initialize(const Callbacks &callbacks);
    void registerCoreTools(const QString &workspaceRoot = {});
    void registerPluginProviders(PluginManager *pluginManager);
    void setWorkspaceRoot(const QString &root);

    AgentController *agentController() const { return m_agentController; }
    ContextBuilder  *contextBuilder() const { return m_contextBuilder; }
    SessionStore    *sessionStore() const { return m_sessionStore; }
    ToolRegistry    *toolRegistry() const { return m_toolRegistry; }

    AgentProviderRegistry *providerRegistry() const { return m_providerRegistry; }
    AgentRequestRouter    *requestRouter() const { return m_requestRouter; }
    ChatSessionService    *chatSessionService() const { return m_chatSessionService; }
    ToolApprovalService   *toolApprovalService() const { return m_toolApprovalService; }
    ProjectBrainService   *brainService() const { return m_brainService; }
    MemorySuggestionEngine *memorySuggestionEngine() const { return m_memorySuggestionEngine; }
    DiagnosticsNotifier    *diagnosticsNotifier() const;
    AgentUIBus             *uiBus() const { return m_uiBus; }

    // Plugin extension accessors (populated by registerPluginProviders)
    const QList<IChatSessionImporter *>      &sessionImporters() const { return m_sessionImporters; }
    const QList<IProviderAuthIntegration *>   &authIntegrations() const { return m_authIntegrations; }
    const QList<IAgentSettingsPageProvider *> &settingsPages() const { return m_settingsPages; }

private:
    AgentOrchestrator *m_orchestrator;
    ServiceRegistry   *m_services;
    IFileSystem       *m_fileSystem;
    Callbacks          m_callbacks;
    std::unique_ptr<IProcess> m_process;

    TerminalSessionManager *m_sessionManager = nullptr;
    ToolRegistry    *m_toolRegistry = nullptr;
    ContextBuilder  *m_contextBuilder = nullptr;
    AgentController *m_agentController = nullptr;
    SessionStore    *m_sessionStore = nullptr;

    AgentProviderRegistry *m_providerRegistry = nullptr;
    AgentRequestRouter    *m_requestRouter = nullptr;
    ChatSessionService    *m_chatSessionService = nullptr;
    ToolApprovalService   *m_toolApprovalService = nullptr;
    ProjectBrainService   *m_brainService = nullptr;
    BrainContextBuilder   *m_brainBuilder = nullptr;
    MemorySuggestionEngine *m_memorySuggestionEngine = nullptr;
    std::unique_ptr<DiagnosticsNotifier> m_diagnosticsNotifier;
    AgentUIBus *m_uiBus = nullptr;

    // Plugin extension registries
    QList<IChatSessionImporter *>      m_sessionImporters;
    QList<IProviderAuthIntegration *>  m_authIntegrations;
    QList<IAgentSettingsPageProvider *> m_settingsPages;
};


#endif /* D5B507CA_8058_4248_8C39_B793CE7AFB48 */
