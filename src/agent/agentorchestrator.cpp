#include "agentorchestrator.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAgent, "exorcist.agent")

AgentOrchestrator::AgentOrchestrator(QObject *parent)
    : QObject(parent)
{
}

AgentOrchestrator::~AgentOrchestrator()
{
    if (m_active)
        m_active->shutdown();
}

// ── Provider registry ─────────────────────────────────────────────────────────

void AgentOrchestrator::registerProvider(IAgentProvider *provider)
{
    if (!provider || m_providers.contains(provider))
        return;

    provider->setParent(this);
    m_providers.append(provider);
    qCInfo(lcAgent) << "Registered agent provider:" << provider->id();
    emit providerRegistered(provider->id());

    // Auto-activate the first provider registered.
    if (!m_active)
        setActiveProvider(provider->id());
}

void AgentOrchestrator::removeProvider(const QString &providerId)
{
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
    return m_providers;
}

IAgentProvider *AgentOrchestrator::provider(const QString &id) const
{
    for (IAgentProvider *p : m_providers) {
        if (p->id() == id)
            return p;
    }
    return nullptr;
}

IAgentProvider *AgentOrchestrator::activeProvider() const
{
    return m_active;
}

void AgentOrchestrator::setActiveProvider(const QString &id)
{
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
    if (!m_active || !m_active->isAvailable()) {
        qCWarning(lcAgent) << "sendRequest: no active/available provider";
        return false;
    }
    m_active->sendRequest(req);
    return true;
}

void AgentOrchestrator::cancelRequest(const QString &requestId)
{
    if (m_active)
        m_active->cancelRequest(requestId);
}

// ── Signal wiring ─────────────────────────────────────────────────────────────

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
