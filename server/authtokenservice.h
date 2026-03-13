#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>

#include <functional>

// ── AuthTokenBridgeService ───────────────────────────────────────────────────
//
// Centralized auth token cache inside ExoBridge.
// Stores API keys and OAuth tokens so one login benefits all IDE instances.
//
// On Windows: uses Windows Credential Manager (DPAPI-backed).
// On other platforms: in-memory cache (shared across instances via IPC).
//
// Methods:
//   "store"    — Store a token     { service, token }
//   "retrieve" — Retrieve a token  { service }
//   "delete"   — Delete a token    { service }
//   "has"      — Check existence   { service }
//   "list"     — List all services

class AuthTokenBridgeService : public QObject
{
    Q_OBJECT

public:
    explicit AuthTokenBridgeService(QObject *parent = nullptr);

    void handleCall(const QString &method,
                    const QJsonObject &args,
                    std::function<void(bool ok, const QJsonObject &result)> respond);

private:
    void doStore(const QJsonObject &args,
                 std::function<void(bool, QJsonObject)> respond);
    void doRetrieve(const QJsonObject &args,
                    std::function<void(bool, QJsonObject)> respond);
    void doDelete(const QJsonObject &args,
                  std::function<void(bool, QJsonObject)> respond);
    void doHas(const QJsonObject &args,
               std::function<void(bool, QJsonObject)> respond);
    void doList(std::function<void(bool, QJsonObject)> respond);

    // Platform credential store helpers
    bool osStoreKey(const QString &service, const QString &token);
    QString osRetrieveKey(const QString &service) const;
    bool osDeleteKey(const QString &service);
    bool osHasKey(const QString &service) const;
    QStringList osListServices() const;

    static QString credentialTarget(const QString &service);

    // In-memory fallback (used on non-Windows or when OS store fails)
    QMap<QString, QString> m_fallback;
    bool m_useFallback = false;
};
