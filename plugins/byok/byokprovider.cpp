#include "byokprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSettings>
#include <QTimer>

ByokProvider::ByokProvider(QObject *parent)
{
    if (parent)
        setParent(parent);
}

QString ByokProvider::id() const
{
    return QStringLiteral("custom");
}

QString ByokProvider::displayName() const
{
    return QStringLiteral("Custom (BYOK)");
}

AgentCapabilities ByokProvider::capabilities() const
{
    return AgentCapability::Chat
         | AgentCapability::Streaming
         | AgentCapability::CodeEdit;
}

bool ByokProvider::isAvailable() const
{
    return m_available;
}

QStringList ByokProvider::availableModels() const
{
    return {m_model};
}

QString ByokProvider::currentModel() const
{
    return m_model;
}

void ByokProvider::setModel(const QString &model)
{
    m_model = model;
}

void ByokProvider::initialize()
{
    // Load configuration from QSettings on first init
    QSettings s;
    s.beginGroup(QStringLiteral("AI"));
    m_endpointUrl = s.value(QStringLiteral("customEndpoint")).toString();
    m_apiKey      = s.value(QStringLiteral("customApiKey")).toString();
    s.endGroup();

    if (m_model.isEmpty())
        m_model = QStringLiteral("default");

    m_available = !m_endpointUrl.isEmpty() && !m_apiKey.isEmpty();
    emit availabilityChanged(m_available);
}

void ByokProvider::shutdown()
{
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void ByokProvider::configure(const QString &endpointUrl, const QString &apiKey)
{
    m_endpointUrl = endpointUrl;
    m_apiKey      = apiKey;
    m_available   = !endpointUrl.isEmpty() && !apiKey.isEmpty();

    if (m_model.isEmpty())
        m_model = QStringLiteral("default");

    emit availabilityChanged(m_available);
}

void ByokProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::AuthError;
        err.message   = tr("Custom provider not configured. Set endpoint URL and API key in Settings.");
        emit responseError(request.requestId, err);
        return;
    }

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("system")},
        {QStringLiteral("content"), QStringLiteral("You are an AI coding assistant.")}
    });

    for (const auto &msg : request.conversationHistory) {
        QString role;
        switch (msg.role) {
        case AgentMessage::Role::System:    role = QStringLiteral("system");    break;
        case AgentMessage::Role::User:      role = QStringLiteral("user");      break;
        case AgentMessage::Role::Assistant:  role = QStringLiteral("assistant"); break;
        case AgentMessage::Role::Tool:      continue;
        }
        messages.append(QJsonObject{
            {QStringLiteral("role"),    role},
            {QStringLiteral("content"), msg.content}
        });
    }

    if (request.appendUserMessage) {
        messages.append(QJsonObject{
            {QStringLiteral("role"),    QStringLiteral("user")},
            {QStringLiteral("content"), buildUserContent(request)}
        });
    }

    const QJsonObject body{
        {QStringLiteral("model"),    m_model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("stream"),   true}
    };

    QNetworkRequest netReq{QUrl{m_endpointUrl}};
    netReq.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/json"));
    netReq.setRawHeader("Authorization",
                        ("Bearer " + m_apiKey).toUtf8());

    m_activeRequestId = request.requestId;
    m_streamAccum.clear();

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply = reply;
    connectReply(reply);
}

void ByokProvider::cancelRequest(const QString &requestId)
{
    if (requestId.isEmpty() || requestId != m_activeRequestId)
        return;
    cleanupActiveReply();
    m_activeRequestId.clear();
}

void ByokProvider::cleanupActiveReply()
{
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

void ByokProvider::connectReply(QNetworkReply *reply)
{
    QPointer<QNetworkReply> safeReply(reply);

    // Stream SSE data lines
    connect(reply, &QNetworkReply::readyRead, this, [this, safeReply] {
        if (!safeReply || safeReply != m_activeReply)
            return;
        const QByteArray raw = safeReply->readAll();
        const QStringList lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));

        for (const QString &line : lines) {
            if (!line.startsWith(QLatin1String("data: ")))
                continue;
            const QString data = line.mid(6).trimmed();
            if (data == QLatin1String("[DONE]")) {
                AgentResponse resp;
                resp.requestId = m_activeRequestId;
                resp.text      = m_streamAccum;
                // Clear state and clean up reply BEFORE emitting
                const QString reqId = m_activeRequestId;
                m_activeRequestId.clear();
                cleanupActiveReply();
                m_streamAccum.clear();
                emit responseFinished(reqId, resp);
                return;
            }

            const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
            const QJsonArray choices = obj[QLatin1String("choices")].toArray();
            if (choices.isEmpty()) continue;

            const QString content =
                choices[0].toObject()[QLatin1String("delta")]
                    .toObject()[QLatin1String("content")].toString();
            if (!content.isEmpty()) {
                m_streamAccum += content;
                emit responseDelta(m_activeRequestId, content);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply)
            return;
        if (m_activeReply != safeReply) {
            // Orphan reply — already cleaned up.
            return;
        }

        // Read everything we need BEFORE cleanup.
        const int status =
            safeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = safeReply->error();
        const QString errorStr = safeReply->errorString();

        cleanupActiveReply();

        if (m_activeRequestId.isEmpty())
            return;

        if (error == QNetworkReply::NoError) {
            AgentResponse resp;
            resp.requestId = m_activeRequestId;
            resp.text = m_streamAccum;
            const QString reqId = m_activeRequestId;
            m_activeRequestId.clear();
            m_streamAccum.clear();
            emit responseFinished(reqId, resp);
        } else {
            AgentError err;
            err.requestId = m_activeRequestId;
            err.message   = errorStr;
            if (status == 401 || status == 403)
                err.code = AgentError::Code::AuthError;
            else if (status == 429)
                err.code = AgentError::Code::RateLimited;
            else
                err.code = AgentError::Code::NetworkError;
            const QString reqId = m_activeRequestId;
            m_activeRequestId.clear();
            m_streamAccum.clear();
            emit responseError(reqId, err);
        }
    });
}

QString ByokProvider::buildUserContent(const AgentRequest &req) const
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
