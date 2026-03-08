#pragma once

#include <QList>
#include <QObject>
#include <QString>

class IAgentProvider;

// ── AgentProviderRegistry ─────────────────────────────────────────────────────
//
// Owns the list of registered AI providers (from plugins or built-in).
// Manages active-provider selection and lifecycle (initialize / shutdown).
//
// Extracted from the former AgentOrchestrator (which was dual-purpose:
// registry + router).  AgentOrchestrator now delegates to this class.
//
// Access via ServiceRegistry: registry->service("agentProviderRegistry")

class AgentProviderRegistry : public QObject
{
    Q_OBJECT

public:
    explicit AgentProviderRegistry(QObject *parent = nullptr);
    ~AgentProviderRegistry() override;

    // ── Registration ──────────────────────────────────────────────────────

    void registerProvider(IAgentProvider *provider);
    void removeProvider(const QString &providerId);

    // ── Query ─────────────────────────────────────────────────────────────

    QList<IAgentProvider *> providers() const;
    IAgentProvider         *provider(const QString &id) const;

    // ── Active provider ───────────────────────────────────────────────────

    IAgentProvider *activeProvider() const;

    /// Switches provider: shutdown old → wire new → initialize new.
    void setActiveProvider(const QString &id);

signals:
    void providerRegistered(const QString &providerId);
    void providerRemoved(const QString &providerId);
    void activeProviderChanged(const QString &providerId);
    void providerAvailabilityChanged(bool available);

private:
    QList<IAgentProvider *> m_providers;
    IAgentProvider         *m_active = nullptr;
};
