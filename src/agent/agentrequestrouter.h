#pragma once

#include <QObject>
#include <QString>

#include "../aiinterface.h"

class AgentProviderRegistry;
class IAgentProvider;

// ── AgentRequestRouter ────────────────────────────────────────────────────────
//
// Routes agent requests to the active provider and forwards provider
// response signals.  Extracted from AgentOrchestrator.
//
// Access via ServiceRegistry: registry->service("agentRequestRouter")

class AgentRequestRouter : public QObject
{
    Q_OBJECT

public:
    explicit AgentRequestRouter(AgentProviderRegistry *registry,
                                QObject *parent = nullptr);

    /// Send a request to the currently active provider.
    bool sendRequest(const AgentRequest &req);

    /// Cancel an in-flight request.
    void cancelRequest(const QString &requestId);

signals:
    // Forwarded from the active provider:
    void responseDelta(const QString &requestId, const QString &textChunk);
    void thinkingDelta(const QString &requestId, const QString &textChunk);
    void responseFinished(const QString &requestId, const AgentResponse &response);
    void responseError(const QString &requestId, const AgentError &error);
    void modelsChanged();

private slots:
    void onActiveProviderChanged(const QString &providerId);

private:
    void wireProvider(IAgentProvider *p);
    void unwireProvider(IAgentProvider *p);

    AgentProviderRegistry *m_registry;
    IAgentProvider        *m_wired = nullptr;
};
