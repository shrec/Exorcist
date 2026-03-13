#include "serviceregistry.h"

ServiceRegistry::ServiceRegistry(QObject *parent)
    : QObject(parent)
{
}

void ServiceRegistry::registerService(const QString &name, QObject *service)
{
    m_services.insert(name, {service, {}});
}

void ServiceRegistry::registerService(const QString &name, QObject *service,
                                       const ServiceContract &contract)
{
    m_services.insert(name, {service, contract});
}

QObject *ServiceRegistry::service(const QString &name) const
{
    auto it = m_services.constFind(name);
    return it != m_services.constEnd() ? it->service : nullptr;
}

QStringList ServiceRegistry::keys() const
{
    return m_services.keys();
}

bool ServiceRegistry::hasService(const QString &name) const
{
    return m_services.contains(name);
}

ServiceContract ServiceRegistry::contract(const QString &name) const
{
    auto it = m_services.constFind(name);
    return it != m_services.constEnd() ? it->contract : ServiceContract{};
}

bool ServiceRegistry::isCompatible(const QString &name, int requiredMajor,
                                    int minMinor) const
{
    auto it = m_services.constFind(name);
    if (it == m_services.constEnd())
        return false;
    const auto &c = it->contract;
    return c.majorVersion == requiredMajor && c.minorVersion >= minMinor;
}
