#pragma once

#include "../sdk/icomponentservice.h"

#include <QObject>

class ComponentRegistry;

/// Adapter that wraps ComponentRegistry behind the IComponentService SDK interface.
class ComponentServiceAdapter : public QObject, public IComponentService
{
    Q_OBJECT

public:
    explicit ComponentServiceAdapter(ComponentRegistry *registry, QObject *parent = nullptr);

    // ── IComponentService ───────────────────────────────────────────

    QStringList availableComponents() const override;
    QString componentDisplayName(const QString &componentId) const override;
    bool supportsMultipleInstances(const QString &componentId) const override;
    bool hasComponent(const QString &componentId) const override;

    QString createInstance(const QString &componentId) override;
    bool destroyInstance(const QString &instanceId) override;

    QStringList activeInstances() const override;
    QStringList instancesOf(const QString &componentId) const override;
    int instanceCount(const QString &componentId) const override;
    QWidget *instanceWidget(const QString &instanceId) const override;

private:
    ComponentRegistry *m_registry;
};
