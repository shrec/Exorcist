#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../aiinterface.h"

// ── AgentOrchestrator ─────────────────────────────────────────────────────────
//
// Central hub that:
//   - Holds the list of registered providers (from plugins or built-in).
//   - Routes requests to the currently-active provider.
//   - Forwards provider signals to subscribers (e.g. the chat panel).
//   - Manages the active provider lifecycle on switch.
//
// Access via ServiceRegistry: registry->service("agentOrchestrator")

class AgentOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit AgentOrchestrator(QObject *parent = nullptr);
    ~AgentOrchestrator() override;

    // ── Provider registry ─────────────────────────────────────────────────
    void registerProvider(IAgentProvider *provider);
    void removeProvider(const QString &providerId);

    QList<IAgentProvider *> providers() const;
    IAgentProvider         *provider(const QString &id) const;
    IAgentProvider         *activeProvider() const;

    // Switches the active provider. Calls shutdown() on the old one (if any)
    // and initialize() on the new one. Emits activeProviderChanged().
    void setActiveProvider(const QString &id);

    // ── Request routing ───────────────────────────────────────────────────
    // Sends a request to the active provider.
    // The requestId inside `req` must be pre-filled (e.g. QUuid::createUuid()).
    // Returns false if no active provider is available.
    bool sendRequest(const AgentRequest &req);
    void cancelRequest(const QString &requestId);

signals:
    // Forwarded from the active provider:
    void responseDelta(const QString &requestId, const QString &textChunk);
    void thinkingDelta(const QString &requestId, const QString &textChunk);
    void responseFinished(const QString &requestId, const AgentResponse &response);
    void responseError(const QString &requestId, const AgentError &error);
    void modelsChanged();

    // Meta events:
    void activeProviderChanged(const QString &providerId);
    void providerRegistered(const QString &providerId);
    void providerRemoved(const QString &providerId);
    void providerAvailabilityChanged(bool available);

private:
    void wireProvider(IAgentProvider *p);
    void unwireProvider(IAgentProvider *p);

    QList<IAgentProvider *> m_providers;
    IAgentProvider         *m_active = nullptr;
};
