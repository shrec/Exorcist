#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QString>

class QNetworkReply;
class SseParser;

class ClaudeProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit ClaudeProvider(QObject *parent = nullptr);

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

private:
    void connectReply(QNetworkReply *reply);
    void fetchModels();
    QString buildUserContent(const AgentRequest &req) const;

    QNetworkAccessManager m_nam;
    SseParser            *m_sseParser;

    QString m_apiKey;
    QString m_model;
    QStringList m_models;
    bool    m_available = false;

    QNetworkReply *m_activeReply = nullptr;
    QString        m_activeRequestId;
    QString        m_accumulated;
};
