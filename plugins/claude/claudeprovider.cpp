#include "claudeprovider.h"
#include "sseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSettings>

static const char kApiUrl[] = "https://api.anthropic.com/v1/messages";

static const char kSystemPrompt[] =
    "You are Claude, an AI coding assistant integrated into the Exorcist IDE. "
    "Help users write, understand, debug, and refactor code. Be concise and "
    "provide working code. When the user shares file context or selected code, "
    "analyze it carefully before responding.";

static const char kAgentSystemPrompt[] =
    "You are Claude acting as a code agent in the Exorcist IDE. "
    "You can read files, propose edits, and help the user implement changes "
    "across their codebase. When asked to make changes, provide complete "
    "code with clear file paths and line references. Think step by step, "
    "consider the full project context, and make precise, targeted edits. "
    "Always explain what you changed and why.";

// ── Constructor ───────────────────────────────────────────────────────────────

ClaudeProvider::ClaudeProvider(QObject *parent)
    : m_sseParser(new SseParser(this))
{
    if (parent)
        setParent(parent);

    m_model = QSettings().value(QStringLiteral("ai/claude/model"),
                                QStringLiteral("claude-sonnet-4-20250514")).toString();

    // Anthropic SSE: text_delta for text, tool_use for tool calls
    connect(m_sseParser, &SseParser::eventReceived,
            this, [this](const QString &eventType, const QString &data) {
        Q_UNUSED(eventType)
        const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
        const QString type = obj[QLatin1String("type")].toString();

        if (type == QLatin1String("content_block_start")) {
            const QJsonObject cb = obj[QLatin1String("content_block")].toObject();
            if (cb[QLatin1String("type")].toString() == QLatin1String("tool_use")) {
                m_inToolUse = true;
                m_currentToolUse = {};
                m_currentToolUse.id   = cb[QLatin1String("id")].toString();
                m_currentToolUse.name = cb[QLatin1String("name")].toString();
            }
        } else if (type == QLatin1String("content_block_delta")) {
            const QJsonObject delta = obj[QLatin1String("delta")].toObject();
            const QString deltaType = delta[QLatin1String("type")].toString();
            if (deltaType == QLatin1String("text_delta")) {
                const QString text = delta[QLatin1String("text")].toString();
                if (!text.isEmpty()) {
                    m_accumulated += text;
                    emit responseDelta(m_activeRequestId, text);
                }
            } else if (deltaType == QLatin1String("input_json_delta")) {
                m_currentToolUse.argumentsJson +=
                    delta[QLatin1String("partial_json")].toString();
            }
        } else if (type == QLatin1String("content_block_stop")) {
            if (m_inToolUse) {
                m_pendingToolCalls.append(m_currentToolUse);
                m_currentToolUse = {};
                m_inToolUse = false;
            }
        } else if (type == QLatin1String("message_stop")
                   || type == QLatin1String("message_delta")) {
            // message_delta with stop_reason="end_turn" or "tool_use"
            const QJsonObject delta = obj[QLatin1String("delta")].toObject();
            const QString stopReason = delta[QLatin1String("stop_reason")].toString();

            if (type == QLatin1String("message_stop")
                || stopReason == QLatin1String("end_turn")
                || stopReason == QLatin1String("tool_use")) {
                AgentResponse resp;
                resp.requestId = m_activeRequestId;
                resp.text      = m_accumulated;

                // Collect tool calls
                for (const auto &ptc : m_pendingToolCalls) {
                    ToolCall tc;
                    tc.id        = ptc.id;
                    tc.name      = ptc.name;
                    tc.arguments = ptc.argumentsJson;
                    resp.toolCalls.append(tc);
                }

                // Extract token usage from message_delta
                const QJsonObject usage = obj[QLatin1String("usage")].toObject();
                if (!usage.isEmpty()) {
                    resp.completionTokens = usage[QLatin1String("output_tokens")].toInt();
                }

                emit responseFinished(m_activeRequestId, resp);
                m_activeRequestId.clear();
                m_activeReply = nullptr;
                m_accumulated.clear();
                m_pendingToolCalls.clear();
            }
        }
    });
}

// ── IAgentProvider identity ───────────────────────────────────────────────────

QString ClaudeProvider::id() const
{
    return QStringLiteral("claude");
}

QString ClaudeProvider::displayName() const
{
    return QStringLiteral("Anthropic Claude");
}

AgentCapabilities ClaudeProvider::capabilities() const
{
    return AgentCapability::Chat
         | AgentCapability::Streaming
         | AgentCapability::CodeEdit
         | AgentCapability::ToolCalling;
}

bool ClaudeProvider::isAvailable() const
{
    return m_available;
}

QStringList ClaudeProvider::availableModels() const
{
    if (!m_models.isEmpty())
        return m_models;
    return {m_model};
}

QString ClaudeProvider::currentModel() const
{
    return m_model;
}

void ClaudeProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        QSettings().setValue(QStringLiteral("ai/claude/model"), model);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ClaudeProvider::initialize()
{
    m_apiKey = QString::fromUtf8(qgetenv("ANTHROPIC_API_KEY"));
    if (m_apiKey.isEmpty())
        m_apiKey = QSettings().value(QStringLiteral("ai/claude/api_key")).toString();

    m_available = !m_apiKey.isEmpty();
    emit availabilityChanged(m_available);

    if (m_available)
        fetchModels();
}

void ClaudeProvider::shutdown()
{
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void ClaudeProvider::fetchModels()
{
    QNetworkRequest req{QUrl{QStringLiteral("https://api.anthropic.com/v1/models")}};
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray data = obj[QLatin1String("data")].toArray();

        QStringList models;
        for (const QJsonValue &v : data) {
            const QString id = v.toObject()[QLatin1String("id")].toString();
            if (!id.isEmpty())
                models.append(id);
        }

        if (!models.isEmpty()) {
            m_models = models;
            if (!m_models.contains(m_model))
                m_model = m_models.first();
            emit modelsChanged();
        }
    });
}

// ── Requests ──────────────────────────────────────────────────────────────────

void ClaudeProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::AuthError;
        err.message   = tr("Claude is not available. Set ANTHROPIC_API_KEY.");
        emit responseError(request.requestId, err);
        return;
    }

    // Build messages (Anthropic format — system is a top-level field)
    QJsonArray messages;
    QString systemText = QString::fromLatin1(
        request.agentMode ? kAgentSystemPrompt : kSystemPrompt);

    for (const auto &msg : request.conversationHistory) {
        switch (msg.role) {
        case AgentMessage::Role::System:
            // Anthropic uses top-level system field, merge all system messages
            if (!msg.content.isEmpty()) {
                if (!systemText.isEmpty())
                    systemText += QStringLiteral("\n\n");
                systemText += msg.content;
            }
            break;

        case AgentMessage::Role::User:
            messages.append(QJsonObject{
                {QStringLiteral("role"),    QStringLiteral("user")},
                {QStringLiteral("content"), msg.content}
            });
            break;

        case AgentMessage::Role::Assistant: {
            // Anthropic format: content is an array of content blocks
            QJsonArray contentBlocks;
            if (!msg.content.isEmpty()) {
                contentBlocks.append(QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("text")},
                    {QStringLiteral("text"), msg.content}
                });
            }
            // Add tool_use blocks for any tool calls
            for (const auto &tc : msg.toolCalls) {
                QJsonObject toolBlock;
                toolBlock[QStringLiteral("type")] = QStringLiteral("tool_use");
                toolBlock[QStringLiteral("id")]   = tc.id;
                toolBlock[QStringLiteral("name")] = tc.name;
                toolBlock[QStringLiteral("input")] =
                    QJsonDocument::fromJson(tc.arguments.toUtf8()).object();
                contentBlocks.append(toolBlock);
            }
            if (!contentBlocks.isEmpty()) {
                messages.append(QJsonObject{
                    {QStringLiteral("role"),    QStringLiteral("assistant")},
                    {QStringLiteral("content"), contentBlocks}
                });
            }
            break;
        }

        case AgentMessage::Role::Tool:
            // Anthropic tool results are user messages with tool_result content
            messages.append(QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), QJsonArray{QJsonObject{
                    {QStringLiteral("type"),        QStringLiteral("tool_result")},
                    {QStringLiteral("tool_use_id"), msg.toolCallId},
                    {QStringLiteral("content"),     msg.content}
                }}}
            });
            break;
        }
    }

    if (request.appendUserMessage) {
        messages.append(QJsonObject{
            {QStringLiteral("role"),    QStringLiteral("user")},
            {QStringLiteral("content"), buildUserContent(request)}
        });
    }

    const QJsonObject body = [&]() {
        QJsonObject b;
        b[QStringLiteral("model")]      = m_model;
        b[QStringLiteral("max_tokens")] = 8192;
        b[QStringLiteral("system")]     = systemText;
        b[QStringLiteral("messages")]   = messages;
        b[QStringLiteral("stream")]     = true;

        // Add tool definitions for agent mode
        if (!request.tools.isEmpty()) {
            QJsonArray tools;
            for (const auto &td : request.tools) {
                QJsonObject toolDef;
                toolDef[QStringLiteral("name")]        = td.name;
                toolDef[QStringLiteral("description")] = td.description;
                toolDef[QStringLiteral("input_schema")] =
                    td.inputSchema.isEmpty()
                        ? QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}
                        : td.inputSchema;
                tools.append(toolDef);
            }
            b[QStringLiteral("tools")] = tools;
        }
        return b;
    }();

    QNetworkRequest netReq{QUrl{QLatin1String(kApiUrl)}};
    netReq.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/json"));
    netReq.setRawHeader("x-api-key", m_apiKey.toUtf8());
    netReq.setRawHeader("anthropic-version", "2023-06-01");

    m_activeRequestId = request.requestId;
    m_accumulated.clear();
    m_pendingToolCalls.clear();
    m_currentToolUse = {};
    m_inToolUse = false;
    m_sseParser->reset();

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply = reply;
    connectReply(reply);
}

void ClaudeProvider::cancelRequest(const QString &requestId)
{
    if (requestId.isEmpty() || requestId != m_activeRequestId)
        return;
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    m_activeRequestId.clear();
    m_accumulated.clear();
    m_pendingToolCalls.clear();
}

// ── Reply wiring ──────────────────────────────────────────────────────────────

void ClaudeProvider::connectReply(QNetworkReply *reply)
{
    QPointer<QNetworkReply> safeReply(reply);

    connect(reply, &QNetworkReply::readyRead, this, [this, safeReply] {
        if (!safeReply || safeReply != m_activeReply)
            return;
        m_sseParser->feed(safeReply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply)
            return;
        safeReply->deleteLater();
        if (m_activeReply != safeReply)
            return;
        m_activeReply = nullptr;

        if (m_activeRequestId.isEmpty())
            return; // Already handled by message_stop event

        const int status =
            safeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (safeReply->error() == QNetworkReply::NoError) {
            AgentResponse resp;
            resp.requestId = m_activeRequestId;
            resp.text      = m_accumulated;
            emit responseFinished(m_activeRequestId, resp);
        } else {
            AgentError err;
            err.requestId = m_activeRequestId;
            err.message   = safeReply->errorString();
            if (status == 401 || status == 403)
                err.code = AgentError::Code::AuthError;
            else if (status == 429)
                err.code = AgentError::Code::RateLimited;
            else
                err.code = AgentError::Code::NetworkError;
            emit responseError(m_activeRequestId, err);
        }
        m_activeRequestId.clear();
        m_accumulated.clear();
    });
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ClaudeProvider::buildUserContent(const AgentRequest &req) const
{
    QString content;

    if (!req.activeFilePath.isEmpty()) {
        content += QStringLiteral("Current file: %1\n").arg(req.activeFilePath);
        if (!req.languageId.isEmpty())
            content += QStringLiteral("Language: %1\n").arg(req.languageId);
    }

    if (!req.selectedText.isEmpty())
        content += QStringLiteral("\nSelected code:\n```\n%1\n```\n\n").arg(req.selectedText);

    content += req.userPrompt;
    return content;
}
