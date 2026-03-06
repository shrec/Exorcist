#pragma once

#include <QList>
#include <QObject>
#include <QString>

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
};
Q_DECLARE_FLAGS(AgentCapabilities, AgentCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(AgentCapabilities)

// ── Context types ─────────────────────────────────────────────────────────────

struct AgentMessage
{
    enum class Role { System, User, Assistant };
    Role    role;
    QString content;
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

struct AgentRequest
{
    QString              requestId;
    AgentIntent          intent = AgentIntent::Chat;
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
    QList<AgentProposedEdit> proposedEdits;
};

struct AgentError
{
    enum class Code { NetworkError, AuthError, RateLimited, Cancelled, Unknown };
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

    virtual void initialize() = 0;
    virtual void shutdown()   = 0;

    // Asynchronous request — results arrive via signals.
    // requestId is provided by the caller (use QUuid::createUuid().toString()).
    virtual void sendRequest(const AgentRequest &request)  = 0;
    virtual void cancelRequest(const QString &requestId)   = 0;

signals:
    // Streaming chunk (empty if provider doesn't stream).
    void responseDelta(const QString &requestId, const QString &textChunk);
    // Final response (always emitted on success, even after streaming).
    void responseFinished(const QString &requestId, const AgentResponse &response);
    // Emitted on failure.
    void responseError(const QString &requestId, const AgentError &error);
    // Provider became available / unavailable (auth expired, config changed, etc.)
    void availabilityChanged(bool available);
};

#define EXORCIST_AGENT_IID "org.exorcist.IAgentProvider/2.0"
Q_DECLARE_INTERFACE(IAgentProvider, EXORCIST_AGENT_IID)
