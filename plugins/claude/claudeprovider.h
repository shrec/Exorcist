#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QPointer>
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
    void cleanupActiveReply();
    void fetchModels();
    QString buildUserContent(const AgentRequest &req) const;
    static int maxOutputTokensForModel(const QString &model);

    QNetworkAccessManager m_nam;
    SseParser            *m_sseParser;

    QString m_apiKey;
    QString m_model;
    QStringList m_models;
    bool    m_available = false;

    QPointer<QNetworkReply> m_activeReply;
    QString        m_activeRequestId;
    QString        m_accumulated;

    // Tool call accumulation during streaming
    struct PendingToolUse {
        QString id;
        QString name;
        QString argumentsJson;
    };
    QList<PendingToolUse>  m_pendingToolCalls;
    PendingToolUse         m_currentToolUse;
    bool                   m_inToolUse = false;
};
