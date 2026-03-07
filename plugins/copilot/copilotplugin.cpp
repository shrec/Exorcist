#include "copilotplugin.h"
#include "copilotprovider.h"

PluginInfo CopilotPlugin::info() const
{
    return {QStringLiteral("copilot"),
            QStringLiteral("GitHub Copilot"),
            QStringLiteral("1.0.0"),
            QStringLiteral("GitHub Copilot AI assistant"),
            QStringLiteral("Exorcist")};
}

void CopilotPlugin::initialize(QObject *) {}
void CopilotPlugin::shutdown() {}

QList<IAgentProvider *> CopilotPlugin::createProviders(QObject *parent)
{
    auto *provider = new CopilotProvider;
    if (parent)
        provider->setParent(parent);
    return {provider};
}
