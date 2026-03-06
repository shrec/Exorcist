#include "serviceregistry.h"

ServiceRegistry::ServiceRegistry(QObject *parent)
    : QObject(parent)
{
}

void ServiceRegistry::registerService(const QString &name, QObject *service)
{
    m_services.insert(name, service);
}

QObject *ServiceRegistry::service(const QString &name) const
{
    return m_services.value(name, nullptr);
}

QStringList ServiceRegistry::keys() const
{
    return m_services.keys();
}
