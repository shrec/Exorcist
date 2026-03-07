#pragma once

#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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
    explicit ErrorStateWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 12, 12, 12);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_iconLabel->setStyleSheet(QStringLiteral("font-size: 32px;"));
        layout->addWidget(m_iconLabel);

        m_messageLabel = new QLabel(this);
        m_messageLabel->setAlignment(Qt::AlignCenter);
        m_messageLabel->setWordWrap(true);
        layout->addWidget(m_messageLabel);

        m_actionBtn = new QPushButton(this);
        m_actionBtn->setVisible(false);
        connect(m_actionBtn, &QPushButton::clicked, this, &ErrorStateWidget::actionClicked);
        layout->addWidget(m_actionBtn, 0, Qt::AlignCenter);

        m_countdownLabel = new QLabel(this);
        m_countdownLabel->setAlignment(Qt::AlignCenter);
        m_countdownLabel->setVisible(false);
        layout->addWidget(m_countdownLabel);

        m_countdownTimer = new QTimer(this);
        m_countdownTimer->setInterval(1000);
        connect(m_countdownTimer, &QTimer::timeout, this, &ErrorStateWidget::updateCountdown);

        hide(); // hidden by default
    }

    void showExpiredToken()
    {
        m_iconLabel->setText(QStringLiteral("\U0001F7E1"));
        m_messageLabel->setText(tr("Your authentication token has expired.\n"
                                   "Please sign in again to continue."));
        m_actionBtn->setText(tr("Sign In"));
        m_actionBtn->setVisible(true);
        m_countdownLabel->setVisible(false);
        m_countdownTimer->stop();
        show();
    }

    void showRateLimit(int retryAfterSeconds)
    {
        m_rateLimitRemaining = retryAfterSeconds;
        m_iconLabel->setText(QStringLiteral("\U0001F534"));
        m_messageLabel->setText(tr("Rate limit reached. Please wait before retrying."));
        m_actionBtn->setVisible(false);
        m_countdownLabel->setVisible(true);
        updateCountdown();
        m_countdownTimer->start();
        show();
    }

    void showOffline()
    {
        m_iconLabel->setText(QStringLiteral("\u26AB"));
        m_messageLabel->setText(tr("No network connection.\n"
                                   "Please check your internet and try again."));
        m_actionBtn->setText(tr("Retry"));
        m_actionBtn->setVisible(true);
        m_countdownLabel->setVisible(false);
        m_countdownTimer->stop();
        show();
    }

    void showSignInPrompt()
    {
        m_iconLabel->setText(QStringLiteral("\U0001F510"));
        m_messageLabel->setText(tr("Sign in to start using AI features."));
        m_actionBtn->setText(tr("Sign In with GitHub"));
        m_actionBtn->setVisible(true);
        m_countdownLabel->setVisible(false);
        m_countdownTimer->stop();
        show();
    }

    void dismiss() { hide(); }

signals:
    void actionClicked();
    void rateLimitExpired();

private:
    void updateCountdown()
    {
        if (m_rateLimitRemaining <= 0) {
            m_countdownTimer->stop();
            m_countdownLabel->setVisible(false);
            emit rateLimitExpired();
            hide();
            return;
        }
        m_countdownLabel->setText(tr("Retry in %1s").arg(m_rateLimitRemaining));
        m_rateLimitRemaining--;
    }

    QLabel *m_iconLabel;
    QLabel *m_messageLabel;
    QPushButton *m_actionBtn;
    QLabel *m_countdownLabel;
    QTimer *m_countdownTimer;
    int m_rateLimitRemaining = 0;
};
