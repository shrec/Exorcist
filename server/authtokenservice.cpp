#include "authtokenservice.h"

#include <QJsonArray>
#include <QStringList>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

static const QString kPrefix = QStringLiteral("ExoBridge/");

AuthTokenBridgeService::AuthTokenBridgeService(QObject *parent)
    : QObject(parent)
{
#ifndef Q_OS_WIN
    m_useFallback = true;
#endif
}

void AuthTokenBridgeService::handleCall(
    const QString &method,
    const QJsonObject &args,
    std::function<void(bool ok, const QJsonObject &result)> respond)
{
    if (method == QLatin1String("store"))
        doStore(args, std::move(respond));
    else if (method == QLatin1String("retrieve"))
        doRetrieve(args, std::move(respond));
    else if (method == QLatin1String("delete"))
        doDelete(args, std::move(respond));
    else if (method == QLatin1String("has"))
        doHas(args, std::move(respond));
    else if (method == QLatin1String("list"))
        doList(std::move(respond));
    else {
        QJsonObject err;
        err[QLatin1String("message")] =
            QStringLiteral("Unknown auth method: %1").arg(method);
        respond(false, err);
    }
}

// ── Method implementations ───────────────────────────────────────────────────

void AuthTokenBridgeService::doStore(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString service = args.value(QLatin1String("service")).toString();
    const QString token   = args.value(QLatin1String("token")).toString();

    if (service.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] = QStringLiteral("Missing 'service'");
        respond(false, err);
        return;
    }

    const bool ok = osStoreKey(service, token);
    QJsonObject result;
    result[QLatin1String("service")] = service;
    result[QLatin1String("stored")]  = ok;
    respond(ok, result);
}

void AuthTokenBridgeService::doRetrieve(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString service = args.value(QLatin1String("service")).toString();

    if (service.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] = QStringLiteral("Missing 'service'");
        respond(false, err);
        return;
    }

    const QString token = osRetrieveKey(service);
    QJsonObject result;
    result[QLatin1String("service")] = service;
    result[QLatin1String("token")]   = token;
    result[QLatin1String("found")]   = !token.isEmpty();
    respond(true, result);
}

void AuthTokenBridgeService::doDelete(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString service = args.value(QLatin1String("service")).toString();

    if (service.isEmpty()) {
        QJsonObject err;
        err[QLatin1String("message")] = QStringLiteral("Missing 'service'");
        respond(false, err);
        return;
    }

    const bool ok = osDeleteKey(service);
    QJsonObject result;
    result[QLatin1String("service")] = service;
    result[QLatin1String("deleted")] = ok;
    respond(ok, result);
}

void AuthTokenBridgeService::doHas(
    const QJsonObject &args,
    std::function<void(bool, QJsonObject)> respond)
{
    const QString service = args.value(QLatin1String("service")).toString();

    QJsonObject result;
    result[QLatin1String("service")] = service;
    result[QLatin1String("exists")]  = osHasKey(service);
    respond(true, result);
}

void AuthTokenBridgeService::doList(
    std::function<void(bool, QJsonObject)> respond)
{
    const QStringList services = osListServices();
    QJsonArray arr;
    for (const auto &s : services)
        arr.append(s);

    QJsonObject result;
    result[QLatin1String("services")] = arr;
    respond(true, result);
}

// ── Platform credential store ────────────────────────────────────────────────

QString AuthTokenBridgeService::credentialTarget(const QString &service)
{
    return kPrefix + service;
}

bool AuthTokenBridgeService::osStoreKey(const QString &service,
                                         const QString &token)
{
#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();
        const QByteArray tokenUtf8 = token.toUtf8();

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(wTarget.c_str());
        cred.CredentialBlobSize = static_cast<DWORD>(tokenUtf8.size());
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(
            const_cast<char *>(tokenUtf8.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        const std::wstring wUser =
            QStringLiteral("ExoBridge").toStdWString();
        cred.UserName = const_cast<LPWSTR>(wUser.c_str());

        if (CredWriteW(&cred, 0))
            return true;
        // Fall through to fallback on error
    }
#endif

    m_fallback[service] = token;
    m_useFallback = true;
    return true;
}

QString AuthTokenBridgeService::osRetrieveKey(const QString &service) const
{
#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();

        PCREDENTIALW pCred = nullptr;
        if (CredReadW(wTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            const QString result = QString::fromUtf8(
                reinterpret_cast<const char *>(pCred->CredentialBlob),
                static_cast<int>(pCred->CredentialBlobSize));
            CredFree(pCred);
            return result;
        }
    }
#endif

    return m_fallback.value(service);
}

bool AuthTokenBridgeService::osDeleteKey(const QString &service)
{
#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();
        if (CredDeleteW(wTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
            m_fallback.remove(service);
            return true;
        }
    }
#endif

    return m_fallback.remove(service) > 0;
}

bool AuthTokenBridgeService::osHasKey(const QString &service) const
{
#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();
        PCREDENTIALW pCred = nullptr;
        if (CredReadW(wTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            CredFree(pCred);
            return true;
        }
    }
#endif

    return m_fallback.contains(service);
}

QStringList AuthTokenBridgeService::osListServices() const
{
    QStringList result;

#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString filter = kPrefix + QStringLiteral("*");
        const std::wstring wFilter = filter.toStdWString();

        DWORD count = 0;
        PCREDENTIALW *pCreds = nullptr;
        if (CredEnumerateW(wFilter.c_str(), 0, &count, &pCreds)) {
            for (DWORD i = 0; i < count; ++i) {
                QString target =
                    QString::fromWCharArray(pCreds[i]->TargetName);
                if (target.startsWith(kPrefix))
                    result << target.mid(kPrefix.size());
            }
            CredFree(pCreds);
        }
        return result;
    }
#endif

    return m_fallback.keys();
}
