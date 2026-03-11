#pragma once

#include <QList>
#include <QObject>

#include <memory>

#include "../aiinterface.h"

class ITool;

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


// ── IAgentToolPlugin ────────────────────────────────────────────────────────
//
// Optional interface for plugins that provide agent tools. Loaded on demand
// based on workspace context. Examples: BrowserPlugin (Playwright),
// DockerPlugin (container management), StaticAnalysisPlugin (clang-tidy).
//
// Plugin loading sequence:
//   1. PluginManager loads the Qt plugin.
//   2. AgentPlatformBootstrap calls qobject_cast<IAgentToolPlugin*>(plugin).
//   3. If non-null, calls createTools() and registers each with ToolRegistry.
//   4. Tools are available to the agent immediately.
//
// Tools are owned by the ToolRegistry (via unique_ptr), not by the plugin.
// The plugin may be unloaded later; tools remain valid because they are
// self-contained (no back-references to the plugin).

class IAgentToolPlugin
{
public:
    virtual ~IAgentToolPlugin() = default;

    /// Human-readable plugin name (e.g. "Browser Automation", "Docker")
    virtual QString toolPluginName() const = 0;

    /// Create tool instances. Called once during registration.
    /// Each tool is registered with the ToolRegistry independently.
    virtual std::vector<std::unique_ptr<ITool>> createTools() = 0;

    /// Optional: contexts this plugin's tools are relevant for.
    /// Empty = always active. Examples: {"web"}, {"docker"}, {"python"}.
    virtual QStringList contexts() const { return {}; }
};

#define EXORCIST_AGENT_TOOL_PLUGIN_IID "org.exorcist.IAgentToolPlugin/1.0"
Q_DECLARE_INTERFACE(IAgentToolPlugin, EXORCIST_AGENT_TOOL_PLUGIN_IID)
