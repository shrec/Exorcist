#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>

class QNetworkReply;

/// A simple OpenAI-compatible "Bring Your Own Key" provider.
/// Reads endpoint URL and API key from QSettings AI/customEndpoint, AI/customApiKey.
class ByokProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit ByokProvider(QObject *parent = nullptr);

    QString           id()           const override;
    QString           displayName()  const override;
    AgentCapabilities capabilities() const override;
    bool              isAvailable()  const override;

    QStringList       availableModels() const override;
    QString           currentModel()    const override;
    void              setModel(const QString &model) override;

    void initialize() override;
    void shutdown()   override;

    void sendRequest(const AgentRequest &request)  override;
    void cancelRequest(const QString &requestId)   override;

    /// Reconfigure endpoint + API key at runtime (called by settings handler).
    Q_INVOKABLE void configure(const QString &endpointUrl, const QString &apiKey);

private:
    void connectReply(QNetworkReply *reply);
    void cleanupActiveReply();
    QString buildUserContent(const AgentRequest &req) const;

    QNetworkAccessManager m_nam;

    QString m_endpointUrl;
    QString m_apiKey;
    QString m_model;
    bool    m_available = false;

    QPointer<QNetworkReply> m_activeReply;
    QString        m_activeRequestId;
    QString        m_streamAccum;
};
