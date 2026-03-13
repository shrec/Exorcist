#pragma once

#include "aiinterface.h"

#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>

class QNetworkReply;

class OllamaProvider : public IAgentProvider
{
    Q_OBJECT

public:
    explicit OllamaProvider(QObject *parent = nullptr);

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
    void fetchModels();
    void processLine(const QByteArray &line);

    QNetworkAccessManager m_nam;

    QString     m_baseUrl;
    QString     m_model;
    QStringList m_models;
    bool        m_available = false;

    QPointer<QNetworkReply> m_activeReply;
    QString        m_activeRequestId;
    QString        m_accumulated;
};
