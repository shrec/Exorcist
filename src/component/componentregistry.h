#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <memory>
#include <vector>

class IComponentFactory;
class IHostServices;
class QWidget;

namespace exdock {
class ExDockWidget;
class DockManager;
}

/// ComponentRegistry — Discovers, loads, and manages component DLLs.
///
/// Components are lightweight shared libraries that provide UI panel
/// factories. The registry:
///  - Scans component directories for DLLs at startup
///  - Loads each DLL via QPluginLoader and extracts IComponentFactory
///  - Creates widget instances on demand
///  - Tracks active instances for lifetime management
///  - Wraps created widgets in ExDockWidget and registers with DockManager
///
/// Thread safety: all methods must be called from the main thread.
class ComponentRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ComponentRegistry(IHostServices *hostServices,
                                exdock::DockManager *dockManager,
                                QObject *parent = nullptr);
    ~ComponentRegistry() override;

    /// Scan a directory for component DLLs and load them.
    /// Returns the number of components successfully loaded.
    int loadComponentsFromDirectory(const QString &dirPath);

    /// Register a component factory directly (for built-in components).
    /// The registry does NOT take ownership — caller must keep the
    /// factory alive for the registry's lifetime.
    bool registerFactory(IComponentFactory *factory);

    // ── Instance management ─────────────────────────────────────────

    /// Create a new instance of a component type.
    /// Returns the instance ID, or empty string on failure.
    QString createInstance(const QString &componentId);

    /// Destroy a specific instance.
    bool destroyInstance(const QString &instanceId);

    /// Destroy all instances of a component type.
    void destroyAllInstances(const QString &componentId);

    // ── Query ───────────────────────────────────────────────────────

    /// All registered component type IDs.
    QStringList availableComponents() const;

    /// Get factory for a component type. Returns nullptr if not found.
    IComponentFactory *factory(const QString &componentId) const;

    /// All active instance IDs.
    QStringList activeInstances() const;

    /// Active instance IDs for a specific component type.
    QStringList instancesOf(const QString &componentId) const;

    /// Get the widget for an active instance. Returns nullptr if not found.
    QWidget *instanceWidget(const QString &instanceId) const;

    /// Number of active instances of a component type.
    int instanceCount(const QString &componentId) const;

    /// Check if a component type is registered.
    bool hasComponent(const QString &componentId) const;

signals:
    /// Emitted when a new component factory is registered.
    void componentRegistered(const QString &componentId);

    /// Emitted when a new instance is created.
    void instanceCreated(const QString &instanceId, const QString &componentId);

    /// Emitted when an instance is destroyed.
    void instanceDestroyed(const QString &instanceId, const QString &componentId);

private:
    struct InstanceInfo {
        QString componentId;
        QWidget *widget = nullptr;
        exdock::ExDockWidget *dock = nullptr;
    };

    IHostServices *m_hostServices;
    exdock::DockManager *m_dockManager;

    // componentId → factory (non-owning for registered, owned by QPluginLoader for loaded)
    QHash<QString, IComponentFactory *> m_factories;

    // instanceId → info
    QHash<QString, InstanceInfo> m_instances;

    // componentId → next instance number
    QHash<QString, int> m_nextInstanceNum;

    // Loaded plugin loaders (own the DLL handles)
    struct LoadedComponent;
    std::vector<std::unique_ptr<LoadedComponent>> m_loadedDlls;

    QString generateInstanceId(const QString &componentId);
};
