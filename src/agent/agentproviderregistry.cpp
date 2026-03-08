#include "agentproviderregistry.h"

#include "../aiinterface.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcProvReg, "exorcist.agent.registry")

AgentProviderRegistry::AgentProviderRegistry(QObject *parent)
    : QObject(parent)
{
}

AgentProviderRegistry::~AgentProviderRegistry()
{
    if (m_active)
        m_active->shutdown();
}

// ── Registration ──────────────────────────────────────────────────────────────

void AgentProviderRegistry::registerProvider(IAgentProvider *provider)
{
    if (!provider || m_providers.contains(provider))
        return;

    provider->setParent(this);
    m_providers.append(provider);
    qCInfo(lcProvReg) << "Registered provider:" << provider->id();
    emit providerRegistered(provider->id());

    if (!m_active)
        setActiveProvider(provider->id());
}

void AgentProviderRegistry::removeProvider(const QString &providerId)
{
    for (int i = 0; i < m_providers.size(); ++i) {
        if (m_providers[i]->id() == providerId) {
            IAgentProvider *p = m_providers.takeAt(i);
            if (p == m_active) {
                m_active->shutdown();
                m_active = m_providers.isEmpty() ? nullptr : m_providers.first();
                if (m_active) {
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

// ── Query ─────────────────────────────────────────────────────────────────────

QList<IAgentProvider *> AgentProviderRegistry::providers() const
{
    return m_providers;
}

IAgentProvider *AgentProviderRegistry::provider(const QString &id) const
{
    for (IAgentProvider *p : m_providers) {
        if (p->id() == id)
            return p;
    }
    return nullptr;
}

IAgentProvider *AgentProviderRegistry::activeProvider() const
{
    return m_active;
}

void AgentProviderRegistry::setActiveProvider(const QString &id)
{
    IAgentProvider *next = provider(id);
    if (!next || next == m_active)
        return;

    if (m_active)
        m_active->shutdown();

    m_active = next;
    m_active->initialize();

    qCInfo(lcProvReg) << "Active provider:" << m_active->id();
    emit activeProviderChanged(m_active->id());
}
