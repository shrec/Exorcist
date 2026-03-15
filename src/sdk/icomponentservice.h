#pragma once

#include <QString>
#include <QStringList>

class QWidget;

/// IComponentService — SDK interface for managing dynamically-loaded components.
///
/// Plugins use this to discover available components, create new instances
/// (e.g., open a second terminal, open a UART monitor for COM3), and query
/// active instances. Components are loaded from DLLs at startup.
///
/// Example usage from a plugin:
///   auto *svc = host->components();
///   QString id = svc->createInstance("exorcist.terminal");
///   // A new terminal dock panel appears.
///   //
///   // For UART monitor with multiple COM ports:
///   svc->createInstance("exorcist.uart-monitor");  // COM1
///   svc->createInstance("exorcist.uart-monitor");  // COM2

class IComponentService
{
public:
    virtual ~IComponentService() = default;

    // ── Discovery ───────────────────────────────────────────────────

    /// All registered component type IDs.
    virtual QStringList availableComponents() const = 0;

    /// Human-readable name for a component type.
    virtual QString componentDisplayName(const QString &componentId) const = 0;

    /// Whether a component supports multiple simultaneous instances.
    virtual bool supportsMultipleInstances(const QString &componentId) const = 0;

    /// Check if a component type is available.
    virtual bool hasComponent(const QString &componentId) const = 0;

    // ── Instance lifecycle ──────────────────────────────────────────

    /// Create a new instance. Returns instance ID, or empty on failure.
    virtual QString createInstance(const QString &componentId) = 0;

    /// Destroy a specific instance.
    virtual bool destroyInstance(const QString &instanceId) = 0;

    // ── Instance query ──────────────────────────────────────────────

    /// All active instance IDs.
    virtual QStringList activeInstances() const = 0;

    /// Active instances of a specific component type.
    virtual QStringList instancesOf(const QString &componentId) const = 0;

    /// Number of active instances of a component type.
    virtual int instanceCount(const QString &componentId) const = 0;

    /// Get the widget for an active instance.
    virtual QWidget *instanceWidget(const QString &instanceId) const = 0;
};
