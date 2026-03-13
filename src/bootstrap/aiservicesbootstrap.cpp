#include "aiservicesbootstrap.h"

#include <QLabel>
#include <QStatusBar>

#include "agent/contextpruner.h"
#include "agent/autocompactor.h"
#include "agent/testscaffold.h"
#include "agent/authmanager.h"
#include "editor/renamesuggestionwidget.h"
#include "agent/securekeystorage.h"
#include "agent/promptfilemanager.h"
#include "editor/mergeconflictresolver.h"
#include "editor/inlinereviewwidget.h"
#include "agent/modelregistry.h"
#include "agent/oauthmanager.h"
#include "agent/domainfilter.h"
#include "git/commitmessagegenerator.h"
#include "editor/multichunkeditor.h"
#include "editor/languagecompletionconfig.h"
#include "agent/contexteditor.h"
#include "agent/trajectoryrecorder.h"
#include "agent/featureflags.h"
#include "agent/notebookstubs.h"
#include "agent/reviewmanager.h"
#include "agent/testgenerator.h"
#include "agent/authstatusindicator.h"
#include "agent/promptfilepicker.h"
#include "agent/chatthemeadapter.h"
#include "agent/apikeymanagerwidget.h"
#include "agent/networkmonitor.h"
#include "mcp/mcpservermanager.h"
#include "search/workspacechunkindex.h"
#include "ui/notificationtoast.h"

AIServicesBootstrap::AIServicesBootstrap(QObject *parent)
    : QObject(parent)
{
}

void AIServicesBootstrap::initialize(QStatusBar *statusBar)
{
    // ── Self-contained services ───────────────────────────────────────────
    m_contextPruner      = new ContextPruner(this);
    m_autoCompactor      = new AutoCompactor(this);
    m_testScaffold       = new TestScaffold(this);
    m_renameSuggestion   = new RenameSuggestionWidget(qobject_cast<QWidget *>(parent()));
    m_mergeResolver      = new MergeConflictResolver(this);
    m_domainFilter       = new DomainFilter(this);
    m_multiChunkEditor   = new MultiChunkEditor(this);
    m_langCompletionConfig = new LanguageCompletionConfig(this);
    m_contextEditor      = new ContextEditor(nullptr);
    m_trajectoryRecorder = new TrajectoryRecorder(this);
    m_featureFlags       = new FeatureFlags(this);
    m_notebookManager    = new NotebookManager(this);
    m_reviewManager      = new ReviewManager(this);
    m_promptPicker       = new PromptFilePicker(qobject_cast<QWidget *>(parent()));
    m_chatTheme          = new ChatThemeAdapter(this);
    m_mcpServerManager   = new MCPServerManager(this);

    // ── Services with external references ─────────────────────────────────
    m_keyStorage         = new SecureKeyStorage(this);
    m_promptFileManager  = new PromptFileManager(this);
    m_commitMsgGen       = new CommitMessageGenerator(this);
    m_modelRegistry      = new ModelRegistry(this);
    m_chunkIndex         = new WorkspaceChunkIndex(this);

    // ── Auth Manager ──────────────────────────────────────────────────────
    m_authManager = new AuthManager(this);

    // ── Inline Review Widget ──────────────────────────────────────────────
    m_inlineReview = new InlineReviewWidget(qobject_cast<QWidget *>(parent()));

    // ── OAuth Manager ─────────────────────────────────────────────────────
    m_oauthManager = new OAuthManager(this);
    connect(m_oauthManager, &OAuthManager::loginSucceeded, this,
            [this](const QString &token) {
        m_keyStorage->storeKey(QStringLiteral("github_oauth"), token);
    });

    // ── Test Generator ────────────────────────────────────────────────────
    m_testGenerator = new TestGenerator(this);

    // ── Auth Status Indicator ─────────────────────────────────────────────
    m_authIndicator = new AuthStatusIndicator(statusBar, this);
    connect(m_oauthManager, &OAuthManager::loginStarted, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::SigningIn); });
    connect(m_oauthManager, &OAuthManager::loginSucceeded, this,
            [this](const QString &) { m_authIndicator->setState(AuthStatusIndicator::SignedIn); });
    connect(m_oauthManager, &OAuthManager::loginFailed, this,
            [this](const QString &) { m_authIndicator->setState(AuthStatusIndicator::SignedOut); });
    connect(m_oauthManager, &OAuthManager::loggedOut, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::SignedOut); });

    // ── API Key Manager Widget ────────────────────────────────────────────
    m_apiKeyManager = new APIKeyManagerWidget(qobject_cast<QWidget *>(parent()));
    connect(m_apiKeyManager, &APIKeyManagerWidget::keySaved, this,
            [this](const QString &provider, const QString &key) {
        m_keyStorage->storeKey(provider, key);
    });
    connect(m_apiKeyManager, &APIKeyManagerWidget::keyRemoved, this,
            [this](const QString &provider) {
        m_keyStorage->deleteKey(provider);
    });

    // ── Network Monitor ───────────────────────────────────────────────────
    m_networkMonitor = new NetworkMonitor(this);
    connect(m_networkMonitor, &NetworkMonitor::wentOffline, this, [this]() {
        m_authIndicator->setState(AuthStatusIndicator::Offline);
    });
    connect(m_networkMonitor, &NetworkMonitor::wentOnline, this, [this]() {
        m_authIndicator->setState(AuthStatusIndicator::SignedIn);
    });

    // ── Workspace Chunk Index ─────────────────────────────────────────────
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexingStarted,
            this, &AIServicesBootstrap::indexingStarted);
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexProgress,
            this, &AIServicesBootstrap::indexProgress);
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexingFinished,
            this, &AIServicesBootstrap::indexingFinished);
}
