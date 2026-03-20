#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

#include "agentsession.h"
#include "icontextprovider.h"
#include "itool.h"

class AgentOrchestrator;
class BrainContextBuilder;
class DiagnosticsNotifier;
class ToolApprovalService;
class ToolRegistry;
class IAgentProvider;
class SessionStore;

// ── AgentController ───────────────────────────────────────────────────────────
//
// The agent loop state machine. This is the heart of the agentic system.
//
// Flow:
//   user request
//     → context build
//     → model request (with tools)
//     → response arrives
//       → if tool_calls: execute tools → feed results → loop back to model
//       → if text only:  final answer
//     → emit result (text + patches)
//
// The controller manages ONE session at a time (per instance).
// For parallel sessions, create multiple AgentController instances.
//
// The controller does NOT talk to the UI directly — it emits signals
// that the chat panel connects to.

class AgentController : public QObject
{
    Q_OBJECT

public:
    explicit AgentController(AgentOrchestrator *orchestrator,
                             ToolRegistry *toolRegistry,
                             IContextProvider *contextProvider,
                             QObject *parent = nullptr);
    ~AgentController() override;

    // ── Session management ────────────────────────────────────────────────

    /// Start a new session (clears any existing one).
    void newSession(bool agentMode);

    /// Get the current session (nullptr if none).
    AgentSession *session() const;

    /// Whether the controller is currently processing a turn.
    bool isBusy() const;

    // ── Send a user message ───────────────────────────────────────────────
    /// Kicks off the agent loop for one user turn.
    void sendMessage(const QString &userMessage,
                     const QString &activeFilePath = {},
                     const QString &selectedText = {},
                     const QString &languageId = {},
                     AgentIntent intent = AgentIntent::Chat,
                     const QList<Attachment> &attachments = {});

    /// Cancel the current turn.
    void cancel();

    // ── Configuration ─────────────────────────────────────────────────────
    int maxStepsPerTurn() const { return m_maxSteps; }
    void setMaxStepsPerTurn(int n) { m_maxSteps = n; }

    AgentToolPermission maxToolPermission() const { return m_maxPermission; }
    void setMaxToolPermission(AgentToolPermission p);
    // Declared in .cpp so it can forward to ToolApprovalService

    void setSystemPrompt(const QString &prompt) { m_systemPrompt = prompt; }
    void setSessionStore(SessionStore *store) { m_store = store; }

    /// Set the centralised tool-approval service. When set, the controller
    /// delegates all approval checks to it instead of using its own built-in
    /// m_confirmFn / m_alwaysAllowedTools / m_maxPermission fields.
    void setToolApprovalService(ToolApprovalService *svc);

    void setBrainContextBuilder(BrainContextBuilder *b) { m_brainBuilder = b; }

    void setDiagnosticsNotifier(DiagnosticsNotifier *n) { m_diagNotifier = n; }

    void setReasoningEffort(const QString &effort) { m_reasoningEffort = effort; }

    /// Tool approval result from user confirmation dialog.
    enum class ToolApproval { Deny, AllowOnce, AllowAlways };

    /// Set a callback that asks the user to approve dangerous tool calls.
    /// Returns the approval decision.
    using ConfirmToolFn = std::function<ToolApproval(const QString &toolName,
                                            const QJsonObject &args,
                                            const QString &description)>;
    void setToolConfirmationCallback(ConfirmToolFn fn);


    /// Clear the per-session "always allow" list (e.g. on new session).
    void clearAlwaysAllowedTools() { m_alwaysAllowedTools.clear(); }

    ToolRegistry *toolRegistry() const { return m_toolRegistry; }

signals:
    // ── Streaming signals ─────────────────────────────────────────────────
    void streamingDelta(const QString &text);
    void turnStarted(const QString &userMessage);
    void stepCompleted(const AgentStep &step);
    void turnFinished(const AgentTurn &turn);
    void turnError(const QString &errorMessage);

    // ── Tool call signals (for UI feedback) ───────────────────────────────
    void toolCallStarted(const QString &toolName, const QJsonObject &args);
    void toolCallFinished(const QString &toolName, const ToolExecResult &result);

    // ── Patch signals ─────────────────────────────────────────────────────
    void patchProposed(const PatchProposal &patch);

    // ── Confirmation request (for dangerous tools) ────────────────────────
    void confirmationRequired(const QString &toolName,
                              const QJsonObject &args,
                              const QString &description);
    /// Emitted after tool execution when files were modified.
    /// The list contains all file paths that were snapshotted (mutated) in this batch.
    void filesChanged(const QStringList &filePaths);

    /// Token usage update from model response.
    void tokenUsageUpdated(int promptTokens, int completionTokens, int totalTokens);
private slots:
    void onResponseDelta(const QString &requestId, const QString &textChunk);
    void onResponseFinished(const QString &requestId, const AgentResponse &response);
    void onResponseError(const QString &requestId, const AgentError &error);

private:
    void executeAgentStep();
    void processToolCalls(const QList<ToolCall> &toolCalls, const QString &modelText = {});
    ToolExecResult executeSingleTool(const ToolCall &tc);
    void sendModelRequest();
    void finishTurn(const QString &finalText, const QList<PatchProposal> &patches = {});
    QList<PatchProposal> extractPatches(const QString &responseText);
    void snapshotFilesForTool(const QString &toolName, const QJsonObject &args);

    AgentOrchestrator  *m_orchestrator;
    ToolRegistry       *m_toolRegistry;
    IContextProvider   *m_contextProvider;

    std::unique_ptr<AgentSession> m_session;

    QString             m_activeRequestId;
    QString             m_streamAccum;    // accumulated streaming text
    bool                m_busy = false;
    bool                m_processingTools = false; // re-entrancy guard
    int                 m_currentStepCount = 0;
    int                 m_maxSteps = 25;   // safety limit per turn
    AgentToolPermission  m_maxPermission = AgentToolPermission::SafeMutate;
    QString              m_systemPrompt;
    SessionStore        *m_store = nullptr;
    ToolApprovalService *m_approvalService = nullptr;
    BrainContextBuilder *m_brainBuilder = nullptr;
    DiagnosticsNotifier  *m_diagNotifier = nullptr;
    ConfirmToolFn        m_confirmFn;
    QSet<QString>        m_alwaysAllowedTools;
    QString              m_reasoningEffort;

    // Editor context for current turn
    QString m_activeFilePath;
    QString m_selectedText;
    QString m_languageId;
    AgentIntent m_pendingIntent = AgentIntent::Chat;
    QList<Attachment> m_attachments;
};
