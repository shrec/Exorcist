#pragma once

#include <QList>
#include <QObject>

#include "../aiinterface.h"

// ── IAgentPlugin ──────────────────────────────────────────────────────────────
//
// Optional interface that an IPlugin can additionally implement to expose
// one or more AI providers to the IDE.
//
// Plugin loading sequence:
//   1. PluginManager loads the Qt plugin (IPlugin).
//   2. It calls qobject_cast<IAgentPlugin*>(plugin).
//   3. If non-null, it calls createProviders() and registers each with
//      the AgentOrchestrator via ServiceRegistry.
//
// The providers are parented to `parent` and the IDE owns their lifetime.

class IAgentPlugin
{
public:
    virtual ~IAgentPlugin() = default;

    // Create and return provider instances. The IDE takes ownership (parent).
    virtual QList<IAgentProvider *> createProviders(QObject *parent) = 0;
};

#define EXORCIST_AGENT_PLUGIN_IID "org.exorcist.IAgentPlugin/1.0"
Q_DECLARE_INTERFACE(IAgentPlugin, EXORCIST_AGENT_PLUGIN_IID)
