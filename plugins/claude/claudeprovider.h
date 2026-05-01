#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkReply;
class SseParser;

// ── ClaudeProvider ────────────────────────────────────────────────────────────
//
// Full Anthropic Messages API integration ("Claude Code"-level).
//
// Features:
//  - POST /v1/messages with x-api-key + anthropic-version headers
//  - SSE streaming (text_delta, thinking_delta, tool_use blocks)
//  - Tool calling: feeds the IDE's ToolRegistry tools through `tools` param
//    and parses tool_use content blocks back to the agent orchestrator
//  - Workspace-aware system prompt (root, language, active file)
//  - Multi-turn conversation history (user/assistant/tool roles)
//  - Configurable model / maxTokens / temperature / reasoning effort
//  - Inline completion (non-stream chat)
//  - Dynamic model discovery from /v1/models
//  - SecureKeyStorage integration (callback-injected, no link dependency)
//  - Capabilities: Chat | Streaming | InlineCompletion | CodeEdit |
//                  ToolCalling | Vision | Thinking
class ClaudeProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit ClaudeProvider(QObject *parent = nullptr);

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

    // Inline (non-chat) completion — emits completionReady on success.
    void requestCompletion(const QString &filePath, const QString &languageId,
                           const QString &textBefore, const QString &textAfter);

    // API key management (used by the claude.editApiKey command).
    Q_INVOKABLE void setApiKey(const QString &key);
    Q_INVOKABLE QString apiKey() const { return m_apiKey; }
    Q_INVOKABLE void clearApiKey();

    // Secure storage injection via callbacks (avoids link-time dependency
    // on core).  Plugin wires these via QMetaObject::invokeMethod.
    using KeyStoreFn    = std::function<bool(const QString &service, const QString &key)>;
    using KeyRetrieveFn = std::function<QString(const QString &service)>;
    using KeyDeleteFn   = std::function<bool(const QString &service)>;
    void setKeyStorageCallbacks(KeyStoreFn store, KeyRetrieveFn retrieve, KeyDeleteFn remove);

signals:
    void completionReady(const QString &suggestion);

private:
    void connectReply(QNetworkReply *reply);
    void cleanupActiveReply();
    void fetchModels();

    QString buildSystemPrompt(const AgentRequest &req) const;
    QString buildUserContent(const AgentRequest &req) const;
    QJsonArray buildTools(const AgentRequest &req) const;
    QJsonArray buildMessages(const AgentRequest &req) const;
    void seedDefaultModels();

    // Per-model output token budget.
    static int maxOutputTokensForModel(const QString &model);
    // True if model identifier matches an Opus / Sonnet 4 thinking-capable line.
    static bool modelSupportsThinking(const QString &model);
    static bool modelSupportsVision  (const QString &model);

    QNetworkAccessManager m_nam;
    SseParser            *m_sseParser;

    QString          m_apiKey;
    QString          m_model;
    QStringList      m_models;        // simple ID list used by availableModels()
    QList<ModelInfo> m_modelInfos;    // full metadata
    bool             m_available = false;

    // Settings (mirrored from QSettings on init / setModel).
    int    m_maxTokensOverride = 0;       // 0 = use model default
    double m_temperature       = 1.0;     // -1 = omit, 0..1 sampler temperature
    bool   m_temperatureSet    = false;
    QString m_userSystemPromptOverride;
    QString m_reasoningEffort;            // "low"|"medium"|"high"|""

    // Active streaming request state.
    QPointer<QNetworkReply> m_activeReply;
    QString        m_activeRequestId;
    QString        m_accumulated;
    QString        m_accumulatedThinking;

    // Tool-call accumulation during streaming (tool_use content blocks
    // arrive with input_json_delta partials).
    struct PendingToolUse {
        QString id;
        QString name;
        QString argumentsJson;
    };
    QList<PendingToolUse>  m_pendingToolCalls;
    PendingToolUse         m_currentToolUse;
    bool                   m_inToolUse = false;

    // Token usage from message_delta.usage.
    int m_promptTokens     = 0;
    int m_completionTokens = 0;

    // Secure credential storage (callbacks injected by plugin).
    KeyStoreFn    m_keyStore;
    KeyRetrieveFn m_keyRetrieve;
    KeyDeleteFn   m_keyDelete;
};
