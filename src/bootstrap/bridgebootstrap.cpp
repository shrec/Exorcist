#include "bridgebootstrap.h"

#include "process/processmanager.h"
#include "process/bridgeclient.h"
#include "serviceregistry.h"

BridgeBootstrap::BridgeBootstrap(QObject *parent)
    : QObject(parent)
{
}

void BridgeBootstrap::initialize(QObject *serviceRegistry)
{
    m_bridgeClient = new BridgeClient(this);
    m_processManager = new ProcessManager(this);
    m_processManager->setBridgeClient(m_bridgeClient);

    connect(m_bridgeClient, &BridgeClient::connected,
            this, &BridgeBootstrap::bridgeConnected);
    connect(m_bridgeClient, &BridgeClient::disconnected,
            this, &BridgeBootstrap::bridgeDisconnected);
    connect(m_bridgeClient, &BridgeClient::bridgeCrashed,
            this, &BridgeBootstrap::bridgeCrashed);

    // Register in ServiceRegistry if available
    if (auto *reg = qobject_cast<ServiceRegistry *>(serviceRegistry)) {
        reg->registerService(QStringLiteral("processManager"), m_processManager);
        reg->registerService(QStringLiteral("bridgeClient"), m_bridgeClient);
    }

    // Connect to the ExoBridge (auto-launches if needed)
    m_bridgeClient->connectToBridge();
}
