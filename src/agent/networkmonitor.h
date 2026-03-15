#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;

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
    explicit NetworkMonitor(QObject *parent = nullptr);

    bool isOnline() const { return m_online; }

    /// Start periodic monitoring. Call only when the user explicitly enables
    /// an AI feature or sends a network request.  Manifesto #9: no network
    /// access without explicit user action.
    void start();
    void stop();

    /// One-shot connectivity check (no periodic timer).
    void checkOnce();

    void setCheckUrl(const QUrl &url);

signals:
    void wentOnline();
    void wentOffline();
    void statusChanged(bool online);

private:
    void check();

    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_timer = nullptr;
    bool m_online = true;
    bool m_checkInFlight = false;
    int  m_failCount = 0;
    QUrl m_checkUrl = QUrl(QStringLiteral("https://api.github.com"));
};
