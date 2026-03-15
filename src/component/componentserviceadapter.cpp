#include "componentserviceadapter.h"
#include "componentregistry.h"
#include "../core/icomponentfactory.h"

ComponentServiceAdapter::ComponentServiceAdapter(ComponentRegistry *registry,
                                                  QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
}

QStringList ComponentServiceAdapter::availableComponents() const
{
    return m_registry->availableComponents();
}

QString ComponentServiceAdapter::componentDisplayName(const QString &componentId) const
{
    auto *fac = m_registry->factory(componentId);
    return fac ? fac->displayName() : QString();
}

bool ComponentServiceAdapter::supportsMultipleInstances(const QString &componentId) const
{
    auto *fac = m_registry->factory(componentId);
    return fac ? fac->supportsMultipleInstances() : false;
}

bool ComponentServiceAdapter::hasComponent(const QString &componentId) const
{
    return m_registry->hasComponent(componentId);
}

QString ComponentServiceAdapter::createInstance(const QString &componentId)
{
    return m_registry->createInstance(componentId);
}

bool ComponentServiceAdapter::destroyInstance(const QString &instanceId)
{
    return m_registry->destroyInstance(instanceId);
}

QStringList ComponentServiceAdapter::activeInstances() const
{
    return m_registry->activeInstances();
}

QStringList ComponentServiceAdapter::instancesOf(const QString &componentId) const
{
    return m_registry->instancesOf(componentId);
}

int ComponentServiceAdapter::instanceCount(const QString &componentId) const
{
    return m_registry->instanceCount(componentId);
}

QWidget *ComponentServiceAdapter::instanceWidget(const QString &instanceId) const
{
    return m_registry->instanceWidget(instanceId);
}
