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

    /// Start periodic monitoring. Call only when the user explicitly enables
    /// an AI feature or sends a network request.  Manifesto #9: no network
    /// access without explicit user action.
    void start() { m_failCount = 0; m_timer->setInterval(30000); m_timer->start(); check(); }
    void stop()  { m_timer->stop(); m_failCount = 0; }

    /// One-shot connectivity check (no periodic timer).
    void checkOnce() { check(); }

    void setCheckUrl(const QUrl &url) { m_checkUrl = url; }

signals:
    void wentOnline();
    void wentOffline();
    void statusChanged(bool online);

private:
    void check()
    {
        if (m_checkInFlight)
            return;
        m_checkInFlight = true;

        QNetworkRequest req(m_checkUrl);
        req.setTransferTimeout(8000);
        auto *reply = m_nam->head(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_checkInFlight = false;
            const bool ok = (reply->error() == QNetworkReply::NoError);

            if (ok) {
                m_failCount = 0;
                m_timer->setInterval(30000); // reset to normal interval
                if (!m_online) {
                    m_online = true;
                    emit statusChanged(true);
                    emit wentOnline();
                }
            } else {
                ++m_failCount;
                // Require 2 consecutive failures before declaring offline
                // to avoid single-packet-drop flapping
                if (m_online && m_failCount >= 2) {
                    m_online = false;
                    emit statusChanged(false);
                    emit wentOffline();
                }
                // Exponential backoff: 30s → 60s → 120s (max 2 min)
                const int backoff = qMin(30000 * (1 << qMin(m_failCount - 1, 2)), 120000);
                m_timer->setInterval(backoff);
            }
        });
    }

    QNetworkAccessManager *m_nam;
    QTimer *m_timer;
    bool m_online = true;
    bool m_checkInFlight = false;
    int  m_failCount = 0;
    QUrl m_checkUrl = QUrl(QStringLiteral("https://api.github.com"));
};
