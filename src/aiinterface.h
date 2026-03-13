#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

// ── Intent ────────────────────────────────────────────────────────────────────

enum class AgentIntent
{
    Chat,
    ExplainCode,
    RefactorSelection,
    GenerateTests,
    FixDiagnostic,
    SuggestCommitMessage,
    TerminalAssist,
    CreateFile,
    CodeReview,
    GenerateCode,      // /generate — produce new code from description
};

// ── Capabilities ──────────────────────────────────────────────────────────────

enum class AgentCapability
{
    Chat             = 1 << 0,
    InlineCompletion = 1 << 1,
    Streaming        = 1 << 2,
    ToolCalling      = 1 << 3,
    CodeEdit         = 1 << 4,
    TestGeneration   = 1 << 5,
    Vision           = 1 << 6,
    Thinking         = 1 << 7,
};
Q_DECLARE_FLAGS(AgentCapabilities, AgentCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(AgentCapabilities)

// ── Model metadata ────────────────────────────────────────────────────────────
// Mirrors IModelAPIResponse from the Copilot Chat extension source.

struct ModelCapabilities
{
    QString type;       // "chat", "completion", "embeddings"
    QString family;     // e.g. "gpt-4o", "claude-sonnet-4"
    bool    streaming        = true;
    bool    toolCalls        = false;
    bool    vision           = false;
    bool    thinking         = false;
    bool    adaptiveThinking = false;
    int     maxPromptTokens       = 0;
    int     maxOutputTokens       = 0;
    int     maxContextWindowTokens = 0;
};

struct ModelBilling
{
    bool    isPremium  = false;
    double  multiplier = 1.0;
};

struct ModelInfo
{
    QString           id;           // "gpt-4o", "claude-sonnet-4-20250514"
    QString           name;         // "GPT-4o", "Claude Sonnet 4"
    QString           vendor;       // "GitHub", "Anthropic"
    QString           version;
    bool              modelPickerEnabled = true;
    bool              isChatDefault      = false;
    bool              isChatFallback     = false;
    bool              isPreview          = false;
    ModelCapabilities capabilities;
    ModelBilling      billing;
    QStringList       supportedEndpoints; // "/chat/completions", "/responses"
};

// ── Tool calling ──────────────────────────────────────────────────────────────

struct ToolParameter
{
    QString     name;
    QString     type;        // "string", "number", "boolean", "object", "array"
    QString     description;
    bool        required = false;
};

struct ToolDefinition
{
    QString               name;        // e.g. "read_file"
    QString               description;
    QList<ToolParameter>  parameters;
    QJsonObject           inputSchema; // Original JSON Schema (preserves items, enum, etc.)
};

struct ToolCall
{
    QString id;        // server-assigned ID for this invocation
    QString name;      // tool name
    QString arguments; // JSON string of arguments
};

struct ToolResult
{
    QString toolCallId;
    QString content;   // result text
    bool    isError = false;
};

// ── Context types ─────────────────────────────────────────────────────────────

struct AgentMessage
{
    enum class Role { System, User, Assistant, Tool };
    Role    role;
    QString content;
    // For tool call results
    QString toolCallId;
    // Tool calls made by the assistant
    QList<ToolCall> toolCalls;
};

struct AgentDiagnostic
{
    enum class Severity { Error, Warning, Info };
    QString  filePath;
    int      line     = -1;
    int      column   = -1;
    QString  message;
    Severity severity = Severity::Error;
};

struct Attachment
{
    enum class Type { File, Image };
    Type    type    = Type::File;
    QString path;       // local file path
    QString mimeType;   // e.g. "image/png"
    QByteArray data;    // raw bytes (for images)
};

struct AgentRequest
{
    QString              requestId;
    AgentIntent          intent = AgentIntent::Chat;
    bool                 agentMode = false; // true = agent can read/edit files
    QString              userPrompt;

    // Editor context
    QString              workspaceRoot;
    QString              activeFilePath;
    QString              languageId;
    QString              selectedText;
    QString              fullFileContent; // populated only for small files
    int                  cursorLine   = -1;
    int                  cursorColumn = -1;

    // Conversation history (for multi-turn chat)
    QList<AgentMessage>  conversationHistory;

    // Diagnostics visible in the editor
    QList<AgentDiagnostic> diagnostics;

    // Tool definitions available in this request
    QList<ToolDefinition>  tools;

    // Thinking / reasoning effort for thinking models ("low", "medium", "high")
    QString reasoningEffort;

    // Image/file attachments for vision models
    QList<Attachment>    attachments;

    // When false, the provider should NOT append a user message at the end.
    // Used for agent tool-continuation calls where history already ends with
    // assistant/tool messages and no extra user message should be injected.
    bool appendUserMessage = true;
};

// ── Response types ────────────────────────────────────────────────────────────

struct AgentProposedEdit
{
    QString filePath;
    int     startLine   = -1;
    int     endLine     = -1;
    QString replacement;
};

struct AgentResponse
{
    QString                  requestId;
    QString                  text;
    QString                  thinkingContent; // reasoning/thinking text (for thinking models)
    QList<AgentProposedEdit> proposedEdits;
    QList<ToolCall>          toolCalls; // tool calls requested by the model
    QStringList              followups;     // suggested follow-up messages

    // Token usage (populated from API response when available)
    int promptTokens     = 0;
    int completionTokens = 0;
    int totalTokens      = 0;
};

struct AgentError
{
    enum class Code { NetworkError, AuthError, RateLimited, ContentFilter, Cancelled, Unknown };
    QString requestId;
    Code    code = Code::Unknown;
    QString message;
};

// ── IAgentProvider ────────────────────────────────────────────────────────────
//
// Base class for all AI providers. Concrete implementations live in plugins.
// Providers are QObjects so they can emit signals for streaming responses.
//
// Lifecycle:
//   initialize() → sendRequest() / cancelRequest()* → shutdown()

class IAgentProvider : public QObject
{
    Q_OBJECT

public:
    ~IAgentProvider() override = default;

    virtual QString           id()           const = 0;
    virtual QString           displayName()  const = 0;
    virtual AgentCapabilities capabilities() const = 0;
    virtual bool              isAvailable()  const = 0;

    // Model selection
    virtual QStringList       availableModels() const = 0;
    virtual QString           currentModel()    const = 0;
    virtual void              setModel(const QString &model) = 0;
    // Full model metadata (empty if provider doesn't support it)
    virtual QList<ModelInfo>  modelInfoList() const { return {}; }
    virtual ModelInfo         currentModelInfo() const { return {}; }

    virtual void initialize() = 0;
    virtual void shutdown()   = 0;

    // Asynchronous request — results arrive via signals.
    // requestId is provided by the caller (use QUuid::createUuid().toString()).
    virtual void sendRequest(const AgentRequest &request)  = 0;
    virtual void cancelRequest(const QString &requestId)   = 0;

signals:
    // Streaming chunk (empty if provider doesn't stream).
    void responseDelta(const QString &requestId, const QString &textChunk);
    // Streaming thinking/reasoning chunk (for thinking models).
    void thinkingDelta(const QString &requestId, const QString &textChunk);
    // Final response (always emitted on success, even after streaming).
    void responseFinished(const QString &requestId, const AgentResponse &response);
    // Emitted on failure.
    void responseError(const QString &requestId, const AgentError &error);
    // Provider became available / unavailable (auth expired, config changed, etc.)
    void availabilityChanged(bool available);
    // Available models list changed (after fetching from server)
    void modelsChanged();
};

#define EXORCIST_AGENT_IID "org.exorcist.IAgentProvider/2.0"
Q_DECLARE_INTERFACE(IAgentProvider, EXORCIST_AGENT_IID)
