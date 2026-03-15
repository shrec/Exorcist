#include "networkmonitor.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

NetworkMonitor::NetworkMonitor(QObject *parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setInterval(30000); // 30s
    connect(m_timer, &QTimer::timeout, this, &NetworkMonitor::check);
}

void NetworkMonitor::start()
{
    m_failCount = 0;
    m_timer->setInterval(30000);
    m_timer->start();
    check();
}

void NetworkMonitor::stop()
{
    m_timer->stop();
    m_failCount = 0;
}

void NetworkMonitor::checkOnce()
{
    check();
}

void NetworkMonitor::setCheckUrl(const QUrl &url)
{
    m_checkUrl = url;
}

void NetworkMonitor::check()
{
    if (m_checkInFlight)
        return;
    m_checkInFlight = true;

    QNetworkRequest req(m_checkUrl);
    req.setTransferTimeout(8000);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
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
