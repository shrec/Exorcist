# Plugin API (Draft)

This document defines the minimal interface for Exorcist plugins. The goal is
to keep the contract stable and small so contributors can build plugins with
low friction.

## Goals
- Simple, explicit lifecycle (initialize/shutdown).
- Minimal required metadata (id, name, version).
- No hard dependency on internal UI classes.

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
