#pragma once

#include <QList>
#include <QQueue>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QWidget>

#include <functional>

#include "../aiinterface.h"
#include "agentsession.h"
#include "markdownrenderer.h"
#include "sessionstore.h"

class QComboBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QScrollArea;
class QTextBrowser;
class QToolButton;
class QUrl;
class QVBoxLayout;
class AgentOrchestrator;
class AgentController;
class ContextBuilder;
class PromptVariableResolver;
class ToolRegistry;

// ── AgentChatPanel ────────────────────────────────────────────────────────────
//
// The AI dock panel. Shows:
//   - Provider selector (combo)
//   - Status indicator (dot: green/grey)
//   - Chat history (QTextBrowser — read-only Markdown-ish display)
//   - Input field + Send / Cancel buttons
//
// When no providers are registered it shows a placeholder message.

class AgentChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AgentChatPanel(AgentOrchestrator *orchestrator,
                            QWidget *parent = nullptr);

    // Inject the agent controller for agentic mode
    void setAgentController(AgentController *controller);
    // Inject session store — will restore the last saved session on attach
    void setSessionStore(SessionStore *store);
    // Inject context builder for context editing
    void setContextBuilder(ContextBuilder *builder) { m_contextBuilder = builder; }

    // Called by MainWindow to inject editor context into the next request.
    void setEditorContext(const QString &filePath,
                          const QString &selectedText,
                          const QString &languageId);
    void setVariableResolver(PromptVariableResolver *resolver);
    void setInsertAtCursorCallback(std::function<void(const QString &)> cb);
    void setRunInTerminalCallback(std::function<void(const QString &)> cb);
    void setWorkspaceFileProvider(std::function<QStringList()> fn);
    void attachSelection(const QString &text, const QString &filePath, int startLine);
    void attachDiagnostics(const QList<AgentDiagnostic> &diagnostics);
    void focusInput();
    void setInputText(const QString &text);
    void setWorkspaceRoot(const QString &root);

signals:
    void reviewAnnotationsReady(const QString &filePath,
                                const QList<QPair<int, QString>> &annotations);
    void openFileRequested(const QString &filePath);

private slots:
    void onSend();
    void onCancel();
    void onModelSelected(int index);
    void onModeSelected(int mode);
    void onProviderRegistered(const QString &id);
    void onProviderRemoved(const QString &id);
    void onActiveProviderChanged(const QString &id);
    void onResponseDelta(const QString &requestId, const QString &chunk);
    void onThinkingDelta(const QString &requestId, const QString &chunk);
    void onResponseFinished(const QString &requestId, const AgentResponse &response);
    void onResponseError(const QString &requestId, const AgentError &error);
    void onAnchorClicked(const QUrl &url);

private:
    void appendMessage(const QString &role, const QString &html);
    void setInputEnabled(bool enabled);
    void refreshProviderList();
    void refreshModelList();
    void setProviderTabActive(const QString &id);
    void setModeButtonActive(int mode);
    void showSlashMenu();
    void hideSlashMenu();
    void showMentionMenu(const QString &trigger, const QString &filter);
    void hideMentionMenu();
    void showChangesBar(int editCount);
    void hideChangesBar();
    QString resolveSlashCommand(const QString &text, AgentIntent &intent) const;

    // Streaming helpers
    void updateStreamingContent();
    void finalizeStreamingBubble(const QString &markdownText, const QString &renderedHtml);
    void showWorkingIndicator(const QString &label = {});
    void hideWorkingIndicator();

    bool eventFilter(QObject *obj, QEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;

    // Message queue helpers
    void checkQueue();

    void addAttachment(const QString &path);
    // Context info strip
    void updateContextInfo();
    // Welcome screen when no session is loaded
    void showEmptyState();
    // Show session history dropdown
    void showSessionHistory();

    AgentOrchestrator *m_orchestrator;
    AgentController   *m_agentController = nullptr;
    ContextBuilder    *m_contextBuilder   = nullptr;
    PromptVariableResolver *m_varResolver = nullptr;
    SessionStore      *m_sessionStore     = nullptr;

    // Chat area
    QTextBrowser *m_history;
    QListWidget  *m_slashMenu;   // slash-command autocomplete popup
    QListWidget  *m_mentionMenu; // @-mention / #-file autocomplete popup
    QString       m_mentionTrigger; // current trigger char: "@" or "#"

    // Changes bar (appears when edits are proposed)
    QWidget     *m_changesBar;
    QLabel      *m_changesLabel;
    QToolButton *m_keepBtn;
    QToolButton *m_undoBtn;

    // Input row
    QPlainTextEdit *m_input;
    QToolButton    *m_sendBtn;
    QToolButton    *m_cancelBtn;
    QToolButton    *m_attachBtn;

    // Attached files/images list
    struct Attachment {
        QString path;    // absolute path (empty = pasted image)
        QByteArray data; // raw bytes for images
        QString name;    // display name
        bool isImage = false;
    };
    QList<Attachment> m_attachments;
    QWidget    *m_attachBar   = nullptr;   // chips row below input
    QHBoxLayout*m_attachLayout = nullptr;

    // Working indicator (shown while model is thinking)
    QWidget *m_workingBar = nullptr;
    QLabel  *m_workingLabel = nullptr;
    QTimer  *m_workingTimer = nullptr;
    int      m_workingDots  = 0;

    // Provider tab bar (top of panel)
    QWidget     *m_providerTabBar;
    QHBoxLayout *m_providerTabLayout;
    QList<QToolButton*> m_providerTabs;

    // Mode pill buttons (Ask / Edit / Agent)
    QToolButton *m_askBtn;
    QToolButton *m_editBtn;
    QToolButton *m_agentBtn;
    int          m_currentMode = 0;
    QMap<int, QString> m_perModeModel; // mode → preferred model ID

    // Bottom bar
    QToolButton *m_newSessionBtn;
    QToolButton *m_historyBtn;
    QComboBox   *m_modelCombo;
    QToolButton *m_ctxBtn;

    // Context info strip (shows active file, token estimate)
    QLabel      *m_contextStrip;

    // Context Window popup
    QFrame      *m_ctxPopup = nullptr;
    void         showContextPopup();
    void         hideContextPopup();

    // Editor context for next request
    QString m_activeFilePath;
    QString m_selectedText;
    QString m_languageId;

    // Tracks the in-flight request so we can stream into the right bubble.
    QString m_pendingRequestId;
    QString m_pendingAccum;
    QString m_thinkingAccum;
    AgentIntent m_pendingIntent = AgentIntent::Chat;

    // Live streaming state
    int  m_streamAnchorPos = 0;   // document char position just before AI bubble
    bool m_streamStarted   = false;

    // Message queue — holds text of messages sent while a request is in flight
    QQueue<QString> m_msgQueue;

    // Proposed patches waiting for user confirmation (Keep / Undo)
    QList<PatchProposal> m_pendingPatches;

    // Test scaffold state
    QString m_pendingTestFilePath;
    QString m_pendingTestCode;

    // Multi-turn conversation history
    QList<AgentMessage> m_conversationHistory;

    // Session ID used when saving Ask-mode turns to SessionStore
    QString m_askSessionId;

    QList<MarkdownRenderer::CodeBlock> m_lastBlocks;
    std::function<void(const QString &)> m_insertAtCursorFn;
    std::function<void(const QString &)> m_runInTerminalFn;
    std::function<QStringList()> m_workspaceFileFn;

    // Feedback / message counter
    int m_messageCount = 0;

    // Input history navigation (Up/Down)
    QStringList m_inputHistory;
    int         m_historyIndex = -1;

    // User messages for retry/edit (indexed by user message count)
    QStringList m_userMessages;
    int         m_userMsgCount = 0;

    // AI messages for copy (indexed by m_messageCount)
    QStringList m_aiMessages;

    // Workspace root for prompt file loading
    QString m_workspaceRoot;
    int m_staticSlashCount = 0; // number of static slash commands
};
