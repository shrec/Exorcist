#pragma once

#include <functional>

#include <QWidget>

#include "aiinterface.h"
#include "agent/agentcontroller.h"
#include "agent/agentsession.h"
#include "chatcontentpart.h"
#include "chatsessionmodel.h"

class QComboBox;
class QHBoxLayout;
class QLabel;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

class AgentOrchestrator;
class AgentController;
class ChatSessionService;
class ContextBuilder;
class PromptVariableResolver;
class SessionStore;

class ChatInputWidget;
class ChatTranscriptView;
class ChatWelcomeWidget;
class ChatSessionHistoryPopup;

// ── ChatPanelWidget ──────────────────────────────────────────────────────────
//
// Composition root for the new chat UI.  Wires together:
//   - ChatSessionModel        (data)
//   - ChatTranscriptView      (transcript scroll area)
//   - ChatInputWidget         (input area)
//   - ChatWelcomeWidget       (empty state)
//   - ChatSessionHistoryPopup (history)
//   - Provider tab bar + models combo
//   - Changes bar
//
// Replaces the monolithic AgentChatPanel with clean, testable pieces.
// All orchestrator / controller communication flows through this widget.

class ChatPanelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPanelWidget(AgentOrchestrator *orchestrator,
                             QWidget *parent = nullptr);

    void setAgentController(AgentController *controller);
    void setSessionStore(SessionStore *store);
    void setChatSessionService(ChatSessionService *svc) { m_chatSessionService = svc; }
    void setContextBuilder(ContextBuilder *builder) { m_contextBuilder = builder; }
    void setVariableResolver(PromptVariableResolver *resolver) { m_varResolver = resolver; }

    void setEditorContext(const QString &filePath,
                          const QString &selectedText,
                          const QString &languageId);

    void setInsertAtCursorCallback(std::function<void(const QString &)> cb)
    {
        m_insertAtCursorFn = std::move(cb);
    }

    void setRunInTerminalCallback(std::function<void(const QString &)> cb)
    {
        m_runInTerminalFn = std::move(cb);
    }

    void setWorkspaceFileProvider(std::function<QStringList()> fn)
    {
        m_workspaceFileFn = std::move(fn);
    }

    void focusInput();
    void setInputText(const QString &text);
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }
    void attachSelection(const QString &text, const QString &filePath, int startLine);
    void attachDiagnostics(const QList<AgentDiagnostic> &diagnostics);

signals:
    void settingsRequested();
    void reviewAnnotationsReady(const QString &filePath,
                                const QList<QPair<int, QString>> &annotations);
    void openFileRequested(const QString &filePath);

private slots:
    void onSend(const QString &text, int mode);
    void onCancel();
    void onProviderRegistered(const QString &id);
    void onProviderRemoved(const QString &id);
    void onActiveProviderChanged(const QString &id);
    void onResponseDelta(const QString &requestId, const QString &chunk);
    void onThinkingDelta(const QString &requestId, const QString &chunk);
    void onResponseFinished(const QString &requestId, const AgentResponse &response);
    void onResponseError(const QString &requestId, const AgentError &error);
    void onFollowupClicked(const QString &message);
    void onToolConfirmed(const QString &callId, bool allowed);
    void onFeedback(const QString &turnId, bool helpful);
    void onNewSession();
    void onShowHistory();
    void onCodeAction(const QUrl &url);

private:
    void buildUi();
    void connectOrchestrator();
    void connectController();
    void connectTranscript();
    void refreshProviderList();
    void refreshModelList();
    void setProviderTabActive(const QString &id);
    void showWelcomeOrTranscript();
    void startRequest(const QString &text, int mode, const QString &slashCmd = {});
    void showChangesBar(int editCount);
    void hideChangesBar();
    void persistCompletedTurn(int turnIndex);
    void restoreSessionTurns(const QJsonArray &completeTurns);
    QString resolveSlashCommand(const QString &text, AgentIntent &intent) const;

    // Context
    AgentOrchestrator     *m_orchestrator     = nullptr;
    AgentController       *m_agentController  = nullptr;
    ContextBuilder        *m_contextBuilder   = nullptr;
    PromptVariableResolver*m_varResolver      = nullptr;
    SessionStore          *m_sessionStore     = nullptr;
    ChatSessionService    *m_chatSessionService = nullptr;

    // Data
    ChatSessionModel      *m_sessionModel     = nullptr;

    // UI
    QVBoxLayout           *m_rootLayout       = nullptr;
    QWidget               *m_providerTabBar   = nullptr;
    QHBoxLayout           *m_providerTabLayout= nullptr;
    QList<QToolButton *>   m_providerTabs;

    QStackedWidget        *m_stack            = nullptr;
    ChatWelcomeWidget     *m_welcome          = nullptr;
    ChatTranscriptView    *m_transcript       = nullptr;
    ChatInputWidget       *m_inputWidget      = nullptr;

    // Changes bar
    QWidget               *m_changesBar       = nullptr;
    QLabel                *m_changesLabel      = nullptr;
    QToolButton           *m_keepBtn           = nullptr;
    QToolButton           *m_undoBtn           = nullptr;

    // History popup
    ChatSessionHistoryPopup *m_historyPopup   = nullptr;

    // Callbacks
    std::function<void(const QString &)> m_insertAtCursorFn;
    std::function<void(const QString &)> m_runInTerminalFn;
    std::function<QStringList()>         m_workspaceFileFn;

    // Editor context
    QString m_activeFilePath;
    QString m_selectedText;
    QString m_languageId;
    QString m_workspaceRoot;

    // In-flight state
    QString     m_pendingRequestId;
    AgentIntent m_pendingIntent = AgentIntent::Chat;
    int         m_currentMode   = 0;

    // Conversation history for multi-turn
    QList<AgentMessage> m_conversationHistory;

    // Pending patches
    QList<PatchProposal> m_pendingPatches;

    // Pending diagnostics for next request
    QList<AgentDiagnostic> m_pendingDiagnostics;

    // Tool confirmation bridge (synchronous callback → async UI)
    QString m_pendingConfirmCallId;
    std::function<void(AgentController::ToolApproval)> m_pendingConfirmResolve;
};
