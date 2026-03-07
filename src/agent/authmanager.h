#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

/// Manages authentication state: token, refresh, status indicator
class AuthManager : public QObject
{
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);

    enum Status { SignedOut, SignedIn, Expired, RateLimited, Offline };
    Q_ENUM(Status)

    Status status() const { return m_status; }
    QString token() const { return m_token; }
    QString statusText() const;
    QString statusIcon() const;

    void setToken(const QString &token);
    void clearToken();

    // Handle 401/429 responses from the API
    void handleHttpStatus(int httpCode);

    // Check network availability
    void checkOnline();

signals:
    void statusChanged(Status status);
    void tokenRefreshed(const QString &newToken);

private:
    void setStatus(Status s);

    Status  m_status = SignedOut;
    QString m_token;
    QTimer *m_retryTimer;
    int     m_retryAfterSecs = 0;
};
