#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;

// ─────────────────────────────────────────────────────────────────────────────
// ErrorStateWidget — shows auth/network error states in the chat panel.
//
// Displays: expired token, rate limit countdown, offline state,
// and sign-in prompt.
// ─────────────────────────────────────────────────────────────────────────────

class ErrorStateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ErrorStateWidget(QWidget *parent = nullptr);

    void showExpiredToken();
    void showRateLimit(int retryAfterSeconds);
    void showOffline();
    void showSignInPrompt();
    void dismiss();

signals:
    void actionClicked();
    void rateLimitExpired();

private:
    void updateCountdown();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_messageLabel = nullptr;
    QPushButton *m_actionBtn = nullptr;
    QLabel *m_countdownLabel = nullptr;
    QTimer *m_countdownTimer = nullptr;
    int m_rateLimitRemaining = 0;
};
