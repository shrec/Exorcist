#pragma once

#include <QObject>
#include <QString>
#include <QMap>

// ─────────────────────────────────────────────────────────────────────────────
// SecureKeyStorage — stores API keys in OS credential store.
//
// On Windows: uses Windows Credential Manager (DPAPI-backed).
// On macOS:   uses Keychain Services (future).
// On Linux:   uses libsecret / Secret Service API (future).
//
// Falls back to in-memory storage if OS store is unavailable.
// ─────────────────────────────────────────────────────────────────────────────

class SecureKeyStorage : public QObject
{
    Q_OBJECT

public:
    explicit SecureKeyStorage(QObject *parent = nullptr);

    /// Store a key. Returns true on success.
    Q_INVOKABLE bool storeKey(const QString &service, const QString &key);

    /// Retrieve a key. Returns empty string if not found.
    Q_INVOKABLE QString retrieveKey(const QString &service) const;

    /// Delete a key. Returns true on success.
    Q_INVOKABLE bool deleteKey(const QString &service);

    /// Check if a key exists.
    Q_INVOKABLE bool hasKey(const QString &service) const;

    /// List all stored service names.
    QStringList services() const;

signals:
    void keyStored(const QString &service);
    void keyDeleted(const QString &service);

private:
    static QString credentialTarget(const QString &service);

    // Fallback in-memory storage (used when OS store unavailable)
    QMap<QString, QString> m_fallback;
    bool m_useFallback = false;
};
