# Plugin API (Draft)

This document defines the minimal interface for Exorcist plugins. The goal is
to keep the contract stable and small so contributors can build plugins with
low friction.

## Goals
- Simple, explicit lifecycle (initialize/shutdown).
- Minimal required metadata (id, name, version).
- No hard dependency on internal UI classes.
- Provider plugins should integrate with shared platform services, not reimplement chat runtime or tools.

## Interface
Plugins implement `IPlugin` and export it via Qt's plugin system.
`initialize()` receives a `ServiceRegistry` pointer as `QObject *services`.

```
struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual void initialize(QObject *services) = 0;
    virtual void shutdown() = 0;
};
```

## Loading
- Exorcist loads plugins from `./plugins` next to the executable.
- Each plugin is a shared library (`.dll`, `.so`, `.dylib`).
- Invalid plugins are skipped and logged.

## Minimal Example (Pseudo)
```
class MyPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.exorcist.IPlugin/1.0")
    Q_INTERFACES(IPlugin)
public:
    PluginInfo info() const override;
    void initialize(QObject *services) override;
    void shutdown() override;
};
```

## Next Steps
- Define and version a stable `ServiceRegistry` interface passed to plugins.
- Add extension points (commands, menus, panels, language tools).
- Publish a plugin template repo.

## AI plugins

Exorcist already supports agent/provider plugins through `IAgentPlugin`.

Detailed authoring guide: `docs/agent-provider-plugin-guide.md`

Current contract:

```cpp
class IAgentPlugin {
public:
        virtual ~IAgentPlugin() = default;
        virtual QList<IAgentProvider *> createProviders(QObject *parent) = 0;
};
```

Related core interfaces:

- `src/aiinterface.h`
- `src/agent/iagentplugin.h`

### Ownership rules

- Plugins own provider-specific integration.
- Core owns the shared chat runtime.
- Core owns the shared tool registry and tool execution pipeline.
- Plugins must not duplicate tool implementations that are already part of the shared platform.

### Recommended future extension points

- `IAgentSettingsPageProvider`
    provider-specific settings UI.
- `IProviderAuthIntegration`
    sign-in/sign-out and auth status integration.
- `IChatSessionImporterPlugin`
    provider-specific remote session import/reconstruction.

### Plugin authoring goal

Adding a new model/backend plugin should mostly mean implementing `IAgentProvider` plus a thin plugin wrapper, while reusing the existing shared tool runtime and chat platform.

### Service expectations

AI plugins should expect to consume services such as:

- provider registry
- tool service
- session service
- auth/config services
- model catalog

Typed service interfaces are preferred over raw string lookups as the plugin API matures.
