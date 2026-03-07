#include "codexplugin.h"
#include "codexprovider.h"

PluginInfo CodexPlugin::info() const
{
    return {QStringLiteral("codex"),
            QStringLiteral("OpenAI Codex"),
            QStringLiteral("1.0.0"),
            QStringLiteral("OpenAI Codex / GPT AI assistant"),
            QStringLiteral("Exorcist")};
}

void CodexPlugin::initialize(QObject *) {}
void CodexPlugin::shutdown() {}

QList<IAgentProvider *> CodexPlugin::createProviders(QObject *parent)
{
    auto *provider = new CodexProvider;
    if (parent)
        provider->setParent(parent);
    return {provider};
}
