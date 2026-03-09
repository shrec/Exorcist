#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>

class ServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ServiceRegistry(QObject *parent = nullptr);

    void registerService(const QString &name, QObject *service);
    Q_INVOKABLE QObject *service(const QString &name) const;
    QStringList keys() const;

    /// Typed convenience — returns nullptr if not found or wrong type.
    /// Usage: auto *ctrl = registry->service<AgentController>("agentController");
    template<typename T>
    T *service(const QString &name) const
    {
        return qobject_cast<T *>(service(name));
    }

private:
    QHash<QString, QObject *> m_services;
};
