#include "ollamaplugin.h"
#include "ollamaprovider.h"

PluginInfo OllamaPlugin::info() const
{
    return {QStringLiteral("ollama"),
            QStringLiteral("Ollama Local LLM"),
            QStringLiteral("1.0.0"),
            QStringLiteral("Local LLM provider via Ollama HTTP API"),
            QStringLiteral("Exorcist")};
}

void OllamaPlugin::initialize(QObject *) {}
void OllamaPlugin::shutdown() {}

QList<IAgentProvider *> OllamaPlugin::createProviders(QObject *parent)
{
    auto *provider = new OllamaProvider;
    if (parent)
        provider->setParent(parent);
    return {provider};
}
