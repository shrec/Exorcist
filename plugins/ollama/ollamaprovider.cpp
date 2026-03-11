#include "ollamaprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

static const char kDefaultUrl[] = "http://localhost:11434";

static const char kSystemPrompt[] =
    "You are a coding assistant integrated into the Exorcist IDE. "
    "Help users write, understand, debug, and refactor code. Be concise and "
    "provide working code.";

// ── Constructor ───────────────────────────────────────────────────────────────

OllamaProvider::OllamaProvider(QObject *parent)
{
    if (parent)
        setParent(parent);

    QSettings s;
    m_baseUrl = s.value(QStringLiteral("ai/ollama/url"),
                        QString::fromLatin1(kDefaultUrl)).toString();
    m_model   = s.value(QStringLiteral("ai/ollama/model"),
                        QStringLiteral("llama3")).toString();
}

// ── Identity ──────────────────────────────────────────────────────────────────

QString OllamaProvider::id() const
{
    return QStringLiteral("ollama");
}

QString OllamaProvider::displayName() const
{
    return QStringLiteral("Ollama (Local)");
}

AgentCapabilities OllamaProvider::capabilities() const
{
    return AgentCapability::Chat
         | AgentCapability::Streaming
         | AgentCapability::CodeEdit;
}

bool OllamaProvider::isAvailable() const
{
    return m_available;
}

QStringList OllamaProvider::availableModels() const
{
    if (!m_models.isEmpty())
        return m_models;
    return {m_model};
}

QString OllamaProvider::currentModel() const
{
    return m_model;
}

void OllamaProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        QSettings().setValue(QStringLiteral("ai/ollama/model"), model);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void OllamaProvider::initialize()
{
    fetchModels();
}

void OllamaProvider::shutdown()
{
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void OllamaProvider::fetchModels()
{
    // GET /api/tags → { "models": [ { "name": "llama3:latest", ... } ] }
    QNetworkRequest req{QUrl{m_baseUrl + QStringLiteral("/api/tags")}};
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_available = false;
            emit availabilityChanged(false);
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray models = obj[QLatin1String("models")].toArray();

        QStringList names;
        for (const QJsonValue &v : models) {
            const QString name = v.toObject()[QLatin1String("name")].toString();
            if (!name.isEmpty())
                names.append(name);
        }

        m_available = true;
        emit availabilityChanged(true);

        if (!names.isEmpty()) {
            m_models = names;
            if (!m_models.contains(m_model))
                m_model = m_models.first();
            emit modelsChanged();
        }
    });
}

// ── Requests ──────────────────────────────────────────────────────────────────

void OllamaProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::NetworkError;
        err.message   = tr("Ollama is not running at %1").arg(m_baseUrl);
        emit responseError(request.requestId, err);
        return;
    }

    // Build messages in OpenAI-compatible format (Ollama /api/chat)
    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("system")},
        {QStringLiteral("content"), QString::fromLatin1(kSystemPrompt)}
    });

    for (const auto &msg : request.conversationHistory) {
        QString role;
        switch (msg.role) {
        case AgentMessage::Role::System:    role = QStringLiteral("system"); break;
        case AgentMessage::Role::User:      role = QStringLiteral("user"); break;
        case AgentMessage::Role::Assistant: role = QStringLiteral("assistant"); break;
        case AgentMessage::Role::Tool:      continue; // Ollama doesn't support tool role
        }
        messages.append(QJsonObject{
            {QStringLiteral("role"),    role},
            {QStringLiteral("content"), msg.content}
        });
    }

    // Build user message with context
    if (request.appendUserMessage) {
        QString userContent = request.userPrompt;
        if (!request.activeFilePath.isEmpty()) {
            userContent = QStringLiteral("File: %1 (language: %2)\n")
                              .arg(request.activeFilePath, request.languageId);
            if (!request.selectedText.isEmpty())
                userContent += QStringLiteral("Selected code:\n```\n%1\n```\n\n").arg(request.selectedText);
            userContent += request.userPrompt;
        }

        messages.append(QJsonObject{
            {QStringLiteral("role"),    QStringLiteral("user")},
            {QStringLiteral("content"), userContent}
        });
    }

    // POST /api/chat (Ollama native streaming API)
    const QJsonObject body{
        {QStringLiteral("model"),    m_model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("stream"),   true}
    };

    QNetworkRequest req{QUrl{m_baseUrl + QStringLiteral("/api/chat")}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    m_activeRequestId = request.requestId;
    m_accumulated.clear();

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply = reply;

    // Ollama streams newline-delimited JSON (NDJSON)
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        while (reply->canReadLine()) {
            const QByteArray line = reply->readLine().trimmed();
            if (!line.isEmpty())
                processLine(line);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_activeReply != reply)
            return;
        m_activeReply = nullptr;

        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError) {
            AgentError err;
            err.requestId = m_activeRequestId;
            err.code      = AgentError::Code::NetworkError;
            err.message   = reply->errorString();
            emit responseError(m_activeRequestId, err);
            m_activeRequestId.clear();
            m_accumulated.clear();
            return;
        }

        // Process any remaining data
        const QByteArray remaining = reply->readAll().trimmed();
        if (!remaining.isEmpty()) {
            for (const QByteArray &line : remaining.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (!trimmed.isEmpty())
                    processLine(trimmed);
            }
        }

        // Emit final response
        if (!m_activeRequestId.isEmpty()) {
            AgentResponse resp;
            resp.requestId = m_activeRequestId;
            resp.text      = m_accumulated;
            emit responseFinished(m_activeRequestId, resp);
        }
        m_activeRequestId.clear();
        m_accumulated.clear();
    });
}

void OllamaProvider::cancelRequest(const QString &requestId)
{
    if (m_activeRequestId == requestId && m_activeReply) {
        m_activeReply->abort();
        m_activeReply = nullptr;
        m_activeRequestId.clear();
        m_accumulated.clear();
    }
}

// ── NDJSON stream processing ──────────────────────────────────────────────────

void OllamaProvider::processLine(const QByteArray &line)
{
    // Each line is a JSON object: { "message": { "content": "..." }, "done": false }
    const QJsonObject obj = QJsonDocument::fromJson(line).object();

    const QJsonObject message = obj[QLatin1String("message")].toObject();
    const QString content = message[QLatin1String("content")].toString();

    if (!content.isEmpty()) {
        m_accumulated += content;
        emit responseDelta(m_activeRequestId, content);
    }

    // "done": true is handled by the finished signal
}
