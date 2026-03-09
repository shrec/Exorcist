#include "claudeplugin.h"
#include "claudeprovider.h"

PluginInfo ClaudePlugin::info() const
{
    return {QStringLiteral("claude"),
            QStringLiteral("Anthropic Claude"),
            QStringLiteral("1.0.0"),
            QStringLiteral("Anthropic Claude AI assistant"),
            QStringLiteral("Exorcist"),
            QStringLiteral("1.0"),
            {PluginPermission::NetworkAccess}};
}

bool ClaudePlugin::initialize(IHostServices *host)
{
    m_host = host;
    return true;
}

void ClaudePlugin::initialize(QObject *) {}
void ClaudePlugin::shutdown() {}

QList<IAgentProvider *> ClaudePlugin::createProviders(QObject *parent)
{
    auto *provider = new ClaudeProvider;
    if (parent)
        provider->setParent(parent);
    return {provider};
}
