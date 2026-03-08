#ifndef D5B507CA_8058_4248_8C39_B793CE7AFB48
#define D5B507CA_8058_4248_8C39_B793CE7AFB48

#include <functional>
#include <memory>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "../aiinterface.h"
#include "../core/iprocess.h"

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
class ToolApprovalService;
class ToolRegistry;

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

    // Plugin extension registries
    QList<IChatSessionImporter *>      m_sessionImporters;
    QList<IProviderAuthIntegration *>  m_authIntegrations;
    QList<IAgentSettingsPageProvider *> m_settingsPages;
};


#endif /* D5B507CA_8058_4248_8C39_B793CE7AFB48 */
