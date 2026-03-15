#include "agentproviderregistry.h"

#include "../aiinterface.h"

#include <QLoggingCategory>
#include <QSettings>

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

    connect(provider, &IAgentProvider::availabilityChanged,
            this, [this, provider](bool available) {
        emit providerAvailabilityChanged(available);
        // Auto-switch: if the current active provider is unavailable
        // and this provider just became available, switch to it.
        if (available && m_active && !m_active->isAvailable() && provider != m_active)
            setActiveProvider(provider->id());
    });

    qCInfo(lcProvReg) << "Registered provider:" << provider->id();
    emit providerRegistered(provider->id());

    // Auto-select: prefer user's last explicit choice (if that provider is
    // actually loaded), otherwise just pick the first registered provider.
    // Each provider is independent — we never block on a missing provider.
    if (!m_active) {
        const QString saved = QSettings().value(
            QStringLiteral("ai/activeProvider")).toString();
        if (!saved.isEmpty() && provider->id() == saved)
            setActiveProvider(saved);
        else
            setActiveProvider(provider->id());
    } else if (m_providers.size() > 1) {
        // Multiple providers loaded — check if the newly arrived one is
        // the user's saved preference and should take over.
        const QString saved = QSettings().value(
            QStringLiteral("ai/activeProvider")).toString();
        if (!saved.isEmpty() && provider->id() == saved && m_active->id() != saved)
            setActiveProvider(saved);
    }
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

    // Persist user's choice so it survives restarts
    QSettings().setValue(QStringLiteral("ai/activeProvider"), m_active->id());

    qCInfo(lcProvReg) << "Active provider:" << m_active->id();
    emit activeProviderChanged(m_active->id());
}
