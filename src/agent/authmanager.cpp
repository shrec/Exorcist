#include "authmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this] {
        if (m_status == RateLimited)
            setStatus(m_token.isEmpty() ? SignedOut : SignedIn);
    });
}

QString AuthManager::statusText() const
{
    switch (m_status) {
    case SignedOut:    return tr("Signed out");
    case SignedIn:     return tr("Active");
    case Expired:      return tr("Token expired — sign in again");
    case RateLimited:  return tr("Rate limited — retry in %1s").arg(m_retryAfterSecs);
    case Offline:      return tr("Offline");
    }
    return {};
}

QString AuthManager::statusIcon() const
{
    switch (m_status) {
    case SignedIn:     return QStringLiteral("●");
    case SignedOut:    return QStringLiteral("○");
    case Expired:     return QStringLiteral("⚠");
    case RateLimited: return QStringLiteral("⏳");
    case Offline:     return QStringLiteral("✕");
    }
    return {};
}

void AuthManager::setToken(const QString &token)
{
    m_token = token;
    setStatus(token.isEmpty() ? SignedOut : SignedIn);
}

void AuthManager::clearToken()
{
    m_token.clear();
    setStatus(SignedOut);
}

void AuthManager::handleHttpStatus(int httpCode)
{
    if (httpCode == 401) {
        setStatus(Expired);
    } else if (httpCode == 429) {
        m_retryAfterSecs = 60; // default 60s retry
        setStatus(RateLimited);
        m_retryTimer->start(m_retryAfterSecs * 1000);
    } else if (httpCode >= 200 && httpCode < 300) {
        if (m_status != SignedIn)
            setStatus(SignedIn);
    }
}

void AuthManager::checkOnline()
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com")));
    req.setTransferTimeout(5000);
    auto *reply = mgr.head(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        if (m_status == Offline)
            setStatus(m_token.isEmpty() ? SignedOut : SignedIn);
    } else {
        setStatus(Offline);
    }
    reply->deleteLater();
}

void AuthManager::setStatus(Status s)
{
    if (m_status == s) return;
    m_status = s;
    emit statusChanged(s);
}
