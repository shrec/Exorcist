#include "byokplugin.h"
#include "byokprovider.h"

PluginInfo ByokPlugin::info() const
{
    return {QStringLiteral("byok"),
            QStringLiteral("Custom (BYOK)"),
            QStringLiteral("1.0.0"),
            QStringLiteral("Bring Your Own Key — any OpenAI-compatible endpoint"),
            QStringLiteral("Exorcist")};
}

void ByokPlugin::initialize(QObject *) {}
void ByokPlugin::shutdown() {}

QList<IAgentProvider *> ByokPlugin::createProviders(QObject *parent)
{
    auto *provider = new ByokProvider;
    if (parent)
        provider->setParent(parent);
    return {provider};
}
