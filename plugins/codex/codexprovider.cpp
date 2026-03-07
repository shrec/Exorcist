#include "codexprovider.h"
#include "sseparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

static const char kDefaultUrl[] = "https://api.openai.com/v1/chat/completions";

static const char kSystemPrompt[] =
    "You are OpenAI Codex, an AI coding assistant integrated into the "
    "Exorcist IDE. Help users write, understand, debug, and refactor code. "
    "Be concise and provide working code. When the user shares file context "
    "or selected code, analyze it carefully before responding.";

static const char kAgentSystemPrompt[] =
    "You are OpenAI acting as a code agent in the Exorcist IDE. "
    "You can read files, propose edits, and help the user implement changes "
    "across their codebase. When asked to make changes, provide complete "
    "code with clear file paths and line references. Think step by step, "
    "consider the full project context, and make precise, targeted edits. "
    "Always explain what you changed and why.";

// ── Constructor ───────────────────────────────────────────────────────────────

CodexProvider::CodexProvider(QObject *parent)
    : m_sseParser(new SseParser(this))
{
    if (parent)
        setParent(parent);

    QSettings s;
    m_model   = s.value(QStringLiteral("ai/openai/model"),
                        QStringLiteral("gpt-4o")).toString();
    m_baseUrl = s.value(QStringLiteral("ai/openai/base_url"),
                        QLatin1String(kDefaultUrl)).toString();

    // SSE data → streaming delta (OpenAI format)
    connect(m_sseParser, &SseParser::eventReceived,
            this, [this](const QString & /*eventType*/, const QString &data) {
        const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
        const QJsonArray choices = obj[QLatin1String("choices")].toArray();
        if (choices.isEmpty())
            return;
        const QString content =
            choices[0].toObject()[QLatin1String("delta")]
                .toObject()[QLatin1String("content")].toString();
        if (!content.isEmpty())
            emit responseDelta(m_activeRequestId, content);
    });

    // SSE [DONE]
    connect(m_sseParser, &SseParser::done, this, [this] {
        AgentResponse resp;
        resp.requestId = m_activeRequestId;
        emit responseFinished(m_activeRequestId, resp);
        m_activeRequestId.clear();
        m_activeReply = nullptr;
    });
}

// ── IAgentProvider identity ───────────────────────────────────────────────────

QString CodexProvider::id() const
{
    return QStringLiteral("codex");
}

QString CodexProvider::displayName() const
{
    return QStringLiteral("OpenAI Codex");
}

AgentCapabilities CodexProvider::capabilities() const
{
    return AgentCapability::Chat
         | AgentCapability::Streaming
         | AgentCapability::CodeEdit
         | AgentCapability::ToolCalling;
}

bool CodexProvider::isAvailable() const
{
    return m_available;
}

QStringList CodexProvider::availableModels() const
{
    if (!m_models.isEmpty())
        return m_models;
    return {m_model};
}

QString CodexProvider::currentModel() const
{
    return m_model;
}

void CodexProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        QSettings().setValue(QStringLiteral("ai/openai/model"), model);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CodexProvider::initialize()
{
    m_apiKey = QString::fromUtf8(qgetenv("OPENAI_API_KEY"));
    if (m_apiKey.isEmpty())
        m_apiKey = QSettings().value(QStringLiteral("ai/openai/api_key")).toString();

    m_available = !m_apiKey.isEmpty();
    emit availabilityChanged(m_available);

    if (m_available)
        fetchModels();
}

void CodexProvider::shutdown()
{
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void CodexProvider::fetchModels()
{
    const QString url = m_baseUrl.contains(QLatin1String("/chat/"))
        ? m_baseUrl.left(m_baseUrl.indexOf(QLatin1String("/chat/"))) + QLatin1String("/models")
        : QStringLiteral("https://api.openai.com/v1/models");

    QNetworkRequest req{QUrl{url}};
    req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
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
            models.sort();
            m_models = models;
            if (!m_models.contains(m_model))
                m_model = m_models.first();
            emit modelsChanged();
        }
    });
}

// ── Requests ──────────────────────────────────────────────────────────────────

void CodexProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::AuthError;
        err.message   = tr("OpenAI is not available. Set OPENAI_API_KEY.");
        emit responseError(request.requestId, err);
        return;
    }

    // Build messages array
    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("system")},
        {QStringLiteral("content"), QString::fromLatin1(
            request.agentMode ? kAgentSystemPrompt : kSystemPrompt)}
    });

    for (const auto &msg : request.conversationHistory) {
        QString role;
        switch (msg.role) {
        case AgentMessage::Role::System:    role = QStringLiteral("system");    break;
        case AgentMessage::Role::User:      role = QStringLiteral("user");      break;
        case AgentMessage::Role::Assistant:  role = QStringLiteral("assistant"); break;
        }
        messages.append(QJsonObject{
            {QStringLiteral("role"),    role},
            {QStringLiteral("content"), msg.content}
        });
    }

    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("user")},
        {QStringLiteral("content"), buildUserContent(request)}
    });

    const QJsonObject body{
        {QStringLiteral("model"),    m_model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("stream"),   true}
    };

    QNetworkRequest netReq{QUrl{m_baseUrl}};
    netReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/json"));
    netReq.setRawHeader("Authorization",
                        ("Bearer " + m_apiKey).toUtf8());

    m_activeRequestId = request.requestId;
    m_sseParser->reset();

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply = reply;
    connectReply(reply);
}

void CodexProvider::cancelRequest(const QString &requestId)
{
    if (requestId.isEmpty() || requestId != m_activeRequestId)
        return;
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    m_activeRequestId.clear();
}

// ── Reply wiring ──────────────────────────────────────────────────────────────

void CodexProvider::connectReply(QNetworkReply *reply)
{
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        m_sseParser->feed(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_activeReply != reply)
            return;
        m_activeReply = nullptr;

        if (m_activeRequestId.isEmpty())
            return; // Already handled by SSE done

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError) {
            AgentResponse resp;
            resp.requestId = m_activeRequestId;
            emit responseFinished(m_activeRequestId, resp);
        } else {
            AgentError err;
            err.requestId = m_activeRequestId;
            err.message   = reply->errorString();
            if (status == 401 || status == 403)
                err.code = AgentError::Code::AuthError;
            else if (status == 429)
                err.code = AgentError::Code::RateLimited;
            else
                err.code = AgentError::Code::NetworkError;
            emit responseError(m_activeRequestId, err);
        }
        m_activeRequestId.clear();
    });
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString CodexProvider::buildUserContent(const AgentRequest &req) const
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
