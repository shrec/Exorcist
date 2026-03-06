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
    QObject *service(const QString &name) const;
    QStringList keys() const;

private:
    QHash<QString, QObject *> m_services;
};
