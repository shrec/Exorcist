#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../aiinterface.h"

class AgentProviderRegistry;
class AgentRequestRouter;

// ── AgentOrchestrator ─────────────────────────────────────────────────────────
//
// Compatibility facade that preserves the original API while delegating to
// the new AgentProviderRegistry (provider management) and AgentRequestRouter
// (request routing / signal forwarding).
//
// When the registry and router are injected via setRegistry() / setRouter(),
// all calls are forwarded.  When they are not set, the orchestrator falls
// back to its own built-in implementation (legacy mode).
//
// Consumers that depend on this class continue to work unchanged.
// New code should prefer using AgentProviderRegistry / AgentRequestRouter
// directly via ServiceRegistry.
//
// Access via ServiceRegistry: registry->service("agentOrchestrator")

class AgentOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit AgentOrchestrator(QObject *parent = nullptr);
    ~AgentOrchestrator() override;

    // ── Delegation injection ──────────────────────────────────────────────
    void setRegistry(AgentProviderRegistry *registry);
    void setRouter(AgentRequestRouter *router);

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
    // Legacy (fallback) signal wiring
    void wireProvider(IAgentProvider *p);
    void unwireProvider(IAgentProvider *p);

    // Delegation targets (nullable — when null, use built-in logic)
    AgentProviderRegistry *m_registry = nullptr;
    AgentRequestRouter    *m_router   = nullptr;

    // Built-in fallback state (used only when m_registry/m_router are null)
    QList<IAgentProvider *> m_providers;
    IAgentProvider         *m_active = nullptr;
};
