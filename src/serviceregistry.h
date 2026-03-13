#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

// ── ServiceContract ──────────────────────────────────────────────────────────
//
// Version metadata attached to a registered service.  Follows semver-like
// rules: major bump = breaking API change, minor bump = additive change.
// Consumers can check compatibility at resolution time.

struct ServiceContract
{
    int majorVersion = 1;
    int minorVersion = 0;
    QString description;
};

// ── ServiceRegistry ──────────────────────────────────────────────────────────

class ServiceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ServiceRegistry(QObject *parent = nullptr);

    // ── Registration ──────────────────────────────────────────────────────

    void registerService(const QString &name, QObject *service);
    void registerService(const QString &name, QObject *service,
                         const ServiceContract &contract);

    // ── Query ─────────────────────────────────────────────────────────────

    Q_INVOKABLE QObject *service(const QString &name) const;
    QStringList keys() const;
    bool hasService(const QString &name) const;

    /// Typed convenience — returns nullptr if not found or wrong type.
    template<typename T>
    T *service(const QString &name) const
    {
        return qobject_cast<T *>(service(name));
    }

    // ── Version / contract ────────────────────────────────────────────────

    ServiceContract contract(const QString &name) const;

    /// True when the service exists and its major version matches
    /// @p requiredMajor while its minor version is >= @p minMinor.
    bool isCompatible(const QString &name, int requiredMajor,
                      int minMinor = 0) const;

private:
    struct Entry {
        QObject        *service = nullptr;
        ServiceContract contract;
    };
    QHash<QString, Entry> m_services;
};
