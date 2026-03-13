#pragma once

#include <QObject>

class QStatusBar;

class ContextPruner;
class AutoCompactor;
class TestScaffold;
class AuthManager;
class RenameSuggestionWidget;
class SecureKeyStorage;
class PromptFileManager;
class MergeConflictResolver;
class InlineReviewWidget;
class ModelRegistry;
class OAuthManager;
class DomainFilter;
class CommitMessageGenerator;
class MultiChunkEditor;
class LanguageCompletionConfig;
class ContextEditor;
class TrajectoryRecorder;
class FeatureFlags;
class NotebookManager;
class ReviewManager;
class TestGenerator;
class AuthStatusIndicator;
class PromptFilePicker;
class ChatThemeAdapter;
class APIKeyManagerWidget;
class NetworkMonitor;
class MCPServerManager;
class WorkspaceChunkIndex;

/// Creates and owns the 30+ AI / utility services that were formerly
/// scattered inside MainWindow::createDockWidgets().
/// Services that need external access provide getter methods.
class AIServicesBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit AIServicesBootstrap(QObject *parent = nullptr);

    /// Create all services. Call from createDockWidgets() or constructor.
    void initialize(QStatusBar *statusBar);

    // ── Accessors for services referenced outside createDockWidgets ──────
    SecureKeyStorage     *keyStorage()         const { return m_keyStorage; }
    PromptFileManager    *promptFileManager()  const { return m_promptFileManager; }
    CommitMessageGenerator *commitMsgGen()     const { return m_commitMsgGen; }
    ModelRegistry        *modelRegistry()      const { return m_modelRegistry; }
    WorkspaceChunkIndex  *chunkIndex()         const { return m_chunkIndex; }
    NetworkMonitor       *networkMonitor()     const { return m_networkMonitor; }
    OAuthManager         *oauthManager()       const { return m_oauthManager; }
    AuthStatusIndicator  *authIndicator()      const { return m_authIndicator; }
    TestGenerator        *testGenerator()      const { return m_testGenerator; }
    InlineReviewWidget   *inlineReview()       const { return m_inlineReview; }
    AuthManager          *authManager()        const { return m_authManager; }

signals:
    void indexingStarted();
    void indexProgress(int done, int total);
    void indexingFinished(int count);

private:
    // ── Self-contained services (no outside references) ──────────────────
    ContextPruner            *m_contextPruner      = nullptr;
    AutoCompactor            *m_autoCompactor       = nullptr;
    TestScaffold             *m_testScaffold        = nullptr;
    AuthManager              *m_authManager         = nullptr;
    RenameSuggestionWidget   *m_renameSuggestion    = nullptr;
    MergeConflictResolver    *m_mergeResolver       = nullptr;
    DomainFilter             *m_domainFilter        = nullptr;
    MultiChunkEditor         *m_multiChunkEditor    = nullptr;
    LanguageCompletionConfig *m_langCompletionConfig = nullptr;
    ContextEditor            *m_contextEditor       = nullptr;
    TrajectoryRecorder       *m_trajectoryRecorder  = nullptr;
    FeatureFlags             *m_featureFlags        = nullptr;
    NotebookManager          *m_notebookManager     = nullptr;
    ReviewManager            *m_reviewManager       = nullptr;
    PromptFilePicker         *m_promptPicker        = nullptr;
    ChatThemeAdapter         *m_chatTheme           = nullptr;
    MCPServerManager         *m_mcpServerManager    = nullptr;

    // ── Services with external references ────────────────────────────────
    SecureKeyStorage         *m_keyStorage          = nullptr;
    PromptFileManager        *m_promptFileManager   = nullptr;
    CommitMessageGenerator   *m_commitMsgGen        = nullptr;
    ModelRegistry            *m_modelRegistry       = nullptr;
    OAuthManager             *m_oauthManager        = nullptr;
    InlineReviewWidget       *m_inlineReview        = nullptr;
    TestGenerator            *m_testGenerator       = nullptr;
    AuthStatusIndicator      *m_authIndicator       = nullptr;
    APIKeyManagerWidget      *m_apiKeyManager       = nullptr;
    NetworkMonitor           *m_networkMonitor      = nullptr;
    WorkspaceChunkIndex      *m_chunkIndex          = nullptr;
};
