#include "agentorchestrator.h"

#include "agentproviderregistry.h"
#include "agentrequestrouter.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAgent, "exorcist.agent")

AgentOrchestrator::AgentOrchestrator(QObject *parent)
    : QObject(parent)
{
}

AgentOrchestrator::~AgentOrchestrator()
{
    if (!m_registry && m_active)
        m_active->shutdown();
}

// ── Delegation injection ──────────────────────────────────────────────────────

void AgentOrchestrator::setRegistry(AgentProviderRegistry *registry)
{
    m_registry = registry;
    if (!m_registry) return;

    // Forward registry signals to our own signals for backward compat
    connect(m_registry, &AgentProviderRegistry::providerRegistered,
            this, &AgentOrchestrator::providerRegistered);
    connect(m_registry, &AgentProviderRegistry::providerRemoved,
            this, &AgentOrchestrator::providerRemoved);
    connect(m_registry, &AgentProviderRegistry::activeProviderChanged,
            this, &AgentOrchestrator::activeProviderChanged);
    connect(m_registry, &AgentProviderRegistry::providerAvailabilityChanged,
            this, &AgentOrchestrator::providerAvailabilityChanged);
}

void AgentOrchestrator::setRouter(AgentRequestRouter *router)
{
    m_router = router;
    if (!m_router) return;

    // Forward router signals to our own signals for backward compat
    connect(m_router, &AgentRequestRouter::responseDelta,
            this, &AgentOrchestrator::responseDelta);
    connect(m_router, &AgentRequestRouter::thinkingDelta,
            this, &AgentOrchestrator::thinkingDelta);
    connect(m_router, &AgentRequestRouter::responseFinished,
            this, &AgentOrchestrator::responseFinished);
    connect(m_router, &AgentRequestRouter::responseError,
            this, &AgentOrchestrator::responseError);
    connect(m_router, &AgentRequestRouter::modelsChanged,
            this, &AgentOrchestrator::modelsChanged);
}

// ── Provider registry ─────────────────────────────────────────────────────────

void AgentOrchestrator::registerProvider(IAgentProvider *provider)
{
    if (m_registry) {
        m_registry->registerProvider(provider);
        return;
    }

    // Legacy fallback
    if (!provider || m_providers.contains(provider))
        return;

    provider->setParent(this);
    m_providers.append(provider);
    qCInfo(lcAgent) << "Registered agent provider:" << provider->id();
    emit providerRegistered(provider->id());

    if (!m_active)
        setActiveProvider(provider->id());
}

void AgentOrchestrator::removeProvider(const QString &providerId)
{
    if (m_registry) {
        m_registry->removeProvider(providerId);
        return;
    }

    // Legacy fallback
    for (int i = 0; i < m_providers.size(); ++i) {
        if (m_providers[i]->id() == providerId) {
            IAgentProvider *p = m_providers.takeAt(i);
            if (p == m_active) {
                unwireProvider(p);
                p->shutdown();
                m_active = m_providers.isEmpty() ? nullptr : m_providers.first();
                if (m_active) {
                    wireProvider(m_active);
                    m_active->initialize();
                    emit activeProviderChanged(m_active->id());
                } else {
                    emit activeProviderChanged({});
                }
            }
            emit providerRemoved(providerId);
            return;
        }
    }
}

QList<IAgentProvider *> AgentOrchestrator::providers() const
{
    if (m_registry)
        return m_registry->providers();
    return m_providers;
}

IAgentProvider *AgentOrchestrator::provider(const QString &id) const
{
    if (m_registry)
        return m_registry->provider(id);

    for (IAgentProvider *p : m_providers) {
        if (p->id() == id)
            return p;
    }
    return nullptr;
}

IAgentProvider *AgentOrchestrator::activeProvider() const
{
    if (m_registry)
        return m_registry->activeProvider();
    return m_active;
}

void AgentOrchestrator::setActiveProvider(const QString &id)
{
    if (m_registry) {
        m_registry->setActiveProvider(id);
        return;
    }

    // Legacy fallback
    IAgentProvider *next = provider(id);
    if (!next || next == m_active)
        return;

    if (m_active) {
        unwireProvider(m_active);
        m_active->shutdown();
    }

    m_active = next;
    wireProvider(m_active);
    m_active->initialize();

    qCInfo(lcAgent) << "Active provider:" << m_active->id();
    emit activeProviderChanged(m_active->id());
}

// ── Request routing ───────────────────────────────────────────────────────────

bool AgentOrchestrator::sendRequest(const AgentRequest &req)
{
    if (m_router)
        return m_router->sendRequest(req);

    // Legacy fallback
    if (!m_active || !m_active->isAvailable()) {
        qCWarning(lcAgent) << "sendRequest: no active/available provider";
        return false;
    }
    m_active->sendRequest(req);
    return true;
}

void AgentOrchestrator::cancelRequest(const QString &requestId)
{
    if (m_router) {
        m_router->cancelRequest(requestId);
        return;
    }

    if (m_active)
        m_active->cancelRequest(requestId);
}

// ── Signal wiring (legacy fallback only) ──────────────────────────────────────

void AgentOrchestrator::wireProvider(IAgentProvider *p)
{
    connect(p, &IAgentProvider::responseDelta,
            this, &AgentOrchestrator::responseDelta);
    connect(p, &IAgentProvider::thinkingDelta,
            this, &AgentOrchestrator::thinkingDelta);
    connect(p, &IAgentProvider::responseFinished,
            this, &AgentOrchestrator::responseFinished);
    connect(p, &IAgentProvider::responseError,
            this, &AgentOrchestrator::responseError);
    connect(p, &IAgentProvider::modelsChanged,
            this, &AgentOrchestrator::modelsChanged);
}

void AgentOrchestrator::unwireProvider(IAgentProvider *p)
{
    disconnect(p, &IAgentProvider::responseDelta,
               this, &AgentOrchestrator::responseDelta);
    disconnect(p, &IAgentProvider::thinkingDelta,
               this, &AgentOrchestrator::thinkingDelta);
    disconnect(p, &IAgentProvider::responseFinished,
               this, &AgentOrchestrator::responseFinished);
    disconnect(p, &IAgentProvider::responseError,
               this, &AgentOrchestrator::responseError);
    disconnect(p, &IAgentProvider::modelsChanged,
               this, &AgentOrchestrator::modelsChanged);
}
