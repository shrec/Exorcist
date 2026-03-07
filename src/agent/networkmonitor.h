#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>

// ─────────────────────────────────────────────────────────────────────────────
// NetworkMonitor — detects online/offline state and auto-recovers.
//
// Periodically pings a known endpoint. When offline → emits wentOffline();
// when restored → emits wentOnline().
// ─────────────────────────────────────────────────────────────────────────────

class NetworkMonitor : public QObject
{
    Q_OBJECT

public:
    explicit NetworkMonitor(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_nam = new QNetworkAccessManager(this);
        m_timer = new QTimer(this);
        m_timer->setInterval(30000); // 30s
        connect(m_timer, &QTimer::timeout, this, &NetworkMonitor::check);
    }

    bool isOnline() const { return m_online; }

    void start() { m_timer->start(); check(); }
    void stop()  { m_timer->stop(); }

    void setCheckUrl(const QUrl &url) { m_checkUrl = url; }

signals:
    void wentOnline();
    void wentOffline();
    void statusChanged(bool online);

private:
    void check()
    {
        QNetworkRequest req(m_checkUrl);
        req.setTransferTimeout(5000);
        auto *reply = m_nam->head(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            const bool ok = (reply->error() == QNetworkReply::NoError);
            if (ok != m_online) {
                m_online = ok;
                emit statusChanged(m_online);
                if (m_online) emit wentOnline();
                else          emit wentOffline();
            }
        });
    }

    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    bool m_online = true;
    QUrl m_checkUrl = QUrl(QStringLiteral("https://api.github.com"));
};
