#pragma once

/// IComponentFactory — Abstract factory for dynamically-loaded component DLLs.
///
/// Components are lightweight shared libraries (.dll / .so / .dylib) that each
/// provide one type of UI panel: terminal, file browser, UART monitor, etc.
///
/// Unlike plugins (which have rich lifecycle, permissions, and manifest),
/// components are simple widget factories. They:
///  - Load via QPluginLoader from the components/ directory
///  - Receive services via IHostServices when creating instances
///  - Support multiple instances (e.g., 3 terminals, 2 UART monitors)
///  - Are profile-aware: profiles list which components to load
///
/// Component DLLs export this interface. The ComponentRegistry discovers
/// and loads them at startup, then creates instances on demand.

#include <QString>
#include <QList>
#include <QtPlugin>

class QWidget;
class IHostServices;

class IComponentFactory
{
public:
    virtual ~IComponentFactory() = default;

    // ── Metadata ────────────────────────────────────────────────────

    /// Unique component type ID (e.g., "exorcist.terminal", "exorcist.uart-monitor").
    virtual QString componentId() const = 0;

    /// Human-readable display name (e.g., "Terminal", "UART Monitor").
    virtual QString displayName() const = 0;

    /// Short description of the component.
    virtual QString description() const = 0;

    /// Icon resource path (e.g., ":/icons/terminal.svg"). Empty if none.
    virtual QString iconPath() const = 0;

    /// Preferred dock area when first created.
    enum DockArea { Left, Right, Bottom, Center };
    virtual DockArea preferredArea() const = 0;

    /// Whether this component supports multiple simultaneous instances.
    /// True for terminals, UART monitors; false for workspace tree.
    virtual bool supportsMultipleInstances() const = 0;

    /// Category for grouping in menus (e.g., "Core", "Debug", "Serial").
    virtual QString category() const = 0;

    // ── Factory ─────────────────────────────────────────────────────

    /// Create a new widget instance for this component.
    ///
    /// @param instanceId   Unique ID for this instance (e.g., "terminal-1").
    ///                     The registry generates this.
    /// @param hostServices SDK services — use to resolve dependencies
    ///                     (git, workspace, search, etc.)
    /// @param parent       Qt parent widget.
    /// @return Newly created widget. Caller takes ownership.
    virtual QWidget *createInstance(const QString &instanceId,
                                    IHostServices *hostServices,
                                    QWidget *parent = nullptr) = 0;

    /// Called when an instance is about to be destroyed. Cleanup hook.
    virtual void destroyInstance(const QString &instanceId)
    {
        Q_UNUSED(instanceId);
    }

    /// Return list of service keys this component requires from
    /// IHostServices. The registry checks availability before loading.
    /// Empty list means no special requirements.
    virtual QList<QString> requiredServices() const { return {}; }
};

#define EXORCIST_COMPONENT_IID "org.exorcist.IComponentFactory/1.0"
Q_DECLARE_INTERFACE(IComponentFactory, EXORCIST_COMPONENT_IID)
