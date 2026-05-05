#pragma once

#include <QDebug>
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
    ///
    /// qobject_cast is the primary path (fast meta-object lookup) but it
    /// fails silently when an SDK interface's MOC is duplicated across DLL
    /// boundaries (multiple Q_OBJECT-generated metaobject identities for
    /// the same class).  When that happens we fall back to dynamic_cast,
    /// which uses C++ RTTI and ignores Qt's metaobject identity — so it
    /// works regardless of MOC duplication.  We log a warning when the
    /// fallback succeeds so future regressions surface in the log instead
    /// of silently null-returning into "F5 doesn't step" territory.
    template<typename T>
    T *service(const QString &name) const
    {
        QObject *raw = service(name);
        if (!raw) return nullptr;
        if (T *typed = qobject_cast<T *>(raw)) return typed;
        if (T *typed = dynamic_cast<T *>(raw)) {
            qWarning().noquote() << "[ServiceRegistry] qobject_cast failed for"
                                 << name << "(MOC duplication?), dynamic_cast succeeded";
            return typed;
        }
        return nullptr;
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
