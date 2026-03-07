#include "claudeplugin.h"
#include "claudeprovider.h"

PluginInfo ClaudePlugin::info() const
{
    return {QStringLiteral("claude"),
            QStringLiteral("Anthropic Claude"),
            QStringLiteral("1.0.0"),
            QStringLiteral("Anthropic Claude AI assistant"),
            QStringLiteral("Exorcist")};
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
