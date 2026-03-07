#include "securekeystorage.h"

#include <QStringList>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

static const QString kPrefix = QStringLiteral("Exorcist_IDE/");

SecureKeyStorage::SecureKeyStorage(QObject *parent)
    : QObject(parent)
{
#ifndef Q_OS_WIN
    // No native credential store on this platform yet — use in-memory fallback
    m_useFallback = true;
#endif
}

QString SecureKeyStorage::credentialTarget(const QString &service)
{
    return kPrefix + service;
}

bool SecureKeyStorage::storeKey(const QString &service, const QString &key)
{
    if (service.isEmpty()) return false;

#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();
        const QByteArray keyUtf8 = key.toUtf8();

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(wTarget.c_str());
        cred.CredentialBlobSize = static_cast<DWORD>(keyUtf8.size());
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(keyUtf8.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        const std::wstring wUser = QStringLiteral("ExorcistUser").toStdWString();
        cred.UserName = const_cast<LPWSTR>(wUser.c_str());

        if (CredWriteW(&cred, 0)) {
            emit keyStored(service);
            return true;
        }
        // Fall through to fallback on error
    }
#endif

    m_fallback[service] = key;
    m_useFallback = true;
    emit keyStored(service);
    return true;
}

QString SecureKeyStorage::retrieveKey(const QString &service) const
{
    if (service.isEmpty()) return {};

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
        // Fall through to fallback
    }
#endif

    return m_fallback.value(service);
}

bool SecureKeyStorage::deleteKey(const QString &service)
{
    if (service.isEmpty()) return false;

#ifdef Q_OS_WIN
    if (!m_useFallback) {
        const QString target = credentialTarget(service);
        const std::wstring wTarget = target.toStdWString();
        if (CredDeleteW(wTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
            m_fallback.remove(service);
            emit keyDeleted(service);
            return true;
        }
    }
#endif

    if (m_fallback.remove(service)) {
        emit keyDeleted(service);
        return true;
    }
    return false;
}

bool SecureKeyStorage::hasKey(const QString &service) const
{
    if (service.isEmpty()) return false;

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

QStringList SecureKeyStorage::services() const
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
                QString target = QString::fromWCharArray(pCreds[i]->TargetName);
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
