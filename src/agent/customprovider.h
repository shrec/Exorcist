#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QString>

class QNetworkReply;

/// A simple OpenAI-compatible "Bring Your Own Key" provider.
/// The user configures the endpoint URL and API key in Settings.
class CustomProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit CustomProvider(QObject *parent = nullptr);

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

    /// Reconfigure endpoint + API key at runtime.
    void configure(const QString &endpointUrl, const QString &apiKey);

private:
    void connectReply(QNetworkReply *reply);
    QString buildUserContent(const AgentRequest &req) const;

    QNetworkAccessManager m_nam;

    QString m_endpointUrl;
    QString m_apiKey;
    QString m_model;
    bool    m_available = false;

    QNetworkReply *m_activeReply     = nullptr;
    QString        m_activeRequestId;
    QString        m_streamAccum;
};
