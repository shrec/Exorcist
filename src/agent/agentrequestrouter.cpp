#include "agentrequestrouter.h"
#include "agentproviderregistry.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRouter, "exorcist.agent.router")

AgentRequestRouter::AgentRequestRouter(AgentProviderRegistry *registry,
                                       QObject *parent)
    : QObject(parent),
      m_registry(registry)
{
    connect(m_registry, &AgentProviderRegistry::activeProviderChanged,
            this, &AgentRequestRouter::onActiveProviderChanged);

    // Wire the current active provider (if already set).
    if (m_registry->activeProvider())
        wireProvider(m_registry->activeProvider());
}

bool AgentRequestRouter::sendRequest(const AgentRequest &req)
{
    IAgentProvider *p = m_registry->activeProvider();
    if (!p || !p->isAvailable()) {
        qCWarning(lcRouter) << "sendRequest: no active/available provider";
        return false;
    }
    p->sendRequest(req);
    return true;
}

void AgentRequestRouter::cancelRequest(const QString &requestId)
{
    IAgentProvider *p = m_registry->activeProvider();
    if (p)
        p->cancelRequest(requestId);
}

void AgentRequestRouter::onActiveProviderChanged(const QString & /*providerId*/)
{
    if (m_wired)
        unwireProvider(m_wired);

    m_wired = m_registry->activeProvider();
    if (m_wired)
        wireProvider(m_wired);
}

void AgentRequestRouter::wireProvider(IAgentProvider *p)
{
    m_wired = p;
    connect(p, &IAgentProvider::responseDelta,
            this, &AgentRequestRouter::responseDelta);
    connect(p, &IAgentProvider::thinkingDelta,
            this, &AgentRequestRouter::thinkingDelta);
    connect(p, &IAgentProvider::responseFinished,
            this, &AgentRequestRouter::responseFinished);
    connect(p, &IAgentProvider::responseError,
            this, &AgentRequestRouter::responseError);
    connect(p, &IAgentProvider::modelsChanged,
            this, &AgentRequestRouter::modelsChanged);
}

void AgentRequestRouter::unwireProvider(IAgentProvider *p)
{
    disconnect(p, &IAgentProvider::responseDelta,
               this, &AgentRequestRouter::responseDelta);
    disconnect(p, &IAgentProvider::thinkingDelta,
               this, &AgentRequestRouter::thinkingDelta);
    disconnect(p, &IAgentProvider::responseFinished,
               this, &AgentRequestRouter::responseFinished);
    disconnect(p, &IAgentProvider::responseError,
               this, &AgentRequestRouter::responseError);
    disconnect(p, &IAgentProvider::modelsChanged,
               this, &AgentRequestRouter::modelsChanged);
}
