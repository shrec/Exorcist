#pragma once

#include "aiinterface.h"
#include "copilottoken.h"

#include <QNetworkAccessManager>
#include <QString>
#include <QTimer>

class QNetworkReply;
class SseParser;

// ── CopilotProvider ───────────────────────────────────────────────────────────
//
// Full GitHub Copilot integration ported from the VS Code Copilot Chat
// extension source (TypeScript → C++17).
//
// Key features:
//  - OAuth Device Flow (GitHub → Copilot API token exchange)
//  - Copilot token parsing with feature flags
//  - Dynamic model discovery from /models endpoint
//  - Full model metadata (capabilities, limits, billing)
//  - Chat Completions API (/chat/completions) with SSE streaming
//  - Responses API (/responses) for models that support it
//  - Tool calling support
//  - Inline completion via chat endpoint
//  - Automatic token refresh (every 25 minutes)

class CopilotProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit CopilotProvider(QObject *parent = nullptr);

    // ── IAgentProvider ────────────────────────────────────────────────────
    QString           id()           const override;
    QString           displayName()  const override;
    AgentCapabilities capabilities() const override;
    bool              isAvailable()  const override;

    QStringList       availableModels() const override;
    QString           currentModel()    const override;
    void              setModel(const QString &model) override;
    QList<ModelInfo>  modelInfoList()    const override;
    ModelInfo         currentModelInfo() const override;

    void initialize() override;
    void shutdown()   override;

    void sendRequest(const AgentRequest &request)  override;
    void cancelRequest(const QString &requestId)   override;

    // Inline completion (non-chat) — returns via signal
    void requestCompletion(const QString &filePath, const QString &languageId,
                           const QString &textBefore, const QString &textAfter);

signals:
    void completionReady(const QString &suggestion);

private:
    // ── Authentication ────────────────────────────────────────────────────
    void tryOAuthLogin();
    void refreshCopilotToken();
    void refreshOAuthToken();           // refresh GitHub OAuth token using refresh_token
    void handleTokenReply(QNetworkReply *reply);
    void handleOAuthRefreshReply(QNetworkReply *reply);
    void scheduleTokenRefresh();
    void clearSavedAuth();

    // ── Model discovery ───────────────────────────────────────────────────
    void fetchModels();
    void handleModelsReply(QNetworkReply *reply);
    ModelInfo parseModelJson(const QJsonObject &obj) const;

    // ── Request building ──────────────────────────────────────────────────
    void sendChatCompletions(const AgentRequest &request);
    void sendResponsesApi(const AgentRequest &request);
    void connectReply(QNetworkReply *reply);
    QJsonArray buildMessages(const AgentRequest &request) const;
    QJsonArray buildTools(const AgentRequest &request) const;
    QString buildUserContent(const AgentRequest &req) const;
    QNetworkRequest makeAuthRequest(const QUrl &url) const;

    // ── SSE parsing ───────────────────────────────────────────────────────
    void handleSseEvent(const QString &eventType, const QString &data);
    void handleChatCompletionsEvent(const QString &data);
    void handleResponsesEvent(const QString &eventType, const QString &data);
    void handleSseDone();

    // ── Retry logic ───────────────────────────────────────────────────────
    void retryOrFail(int httpStatus, const QString &errorMsg);

    // ── State ─────────────────────────────────────────────────────────────
    QNetworkAccessManager m_nam;
    SseParser            *m_sseParser;
    QTimer                m_refreshTimer;
    QTimer                m_idleTimer;        // per-request idle timeout

    // Auth
    QString       m_githubToken;    // GitHub PAT or OAuth token
    QString       m_refreshToken;   // OAuth refresh token (for token renewal)
    CopilotToken  m_token;          // Parsed Copilot API token
    bool          m_available = false;
    bool          m_oauthRefreshAttempted = false;  // prevent infinite refresh loops

    // Models
    QString          m_model;       // Current model ID
    QList<ModelInfo>  m_modelInfos; // Full metadata from /models
    qint64            m_lastModelFetch = 0;

    // Active request
    QNetworkReply *m_activeReply = nullptr;
    QString        m_activeRequestId;
    QString        m_streamAccum;            // accumulated text from streaming deltas
    bool           m_useResponsesApi = false; // true when using /responses endpoint
    // Accumulated tool calls during streaming
    QMap<int, ToolCall> m_pendingToolCalls;
    // Pending tool call for Responses API (one at a time)
    ToolCall       m_responsesToolCall;
    // Accumulated thinking/reasoning content during streaming
    QString m_pendingThinking;

    // Usage tracking
    int m_promptTokens     = 0;
    int m_completionTokens = 0;
    int m_totalTokens      = 0;

    // Retry state
    AgentRequest   m_lastRequest;   // saved for retry
    int            m_retryCount = 0;
    static constexpr int kMaxRetries = 3;
};
