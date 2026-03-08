#pragma once

#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatthemetokens.h"

// ── ChatWelcomeWidget ─────────────────────────────────────────────────────────
//
// The empty-state / welcome view shown when no conversation is active.
// Shows a branding area and quick-start suggestions.

class ChatWelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    enum class State { Default, AuthRequired, RateLimited, Offline, NoProvider };

    explicit ChatWelcomeWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(20, 40, 20, 20);
        m_layout->setSpacing(12);
        m_layout->setAlignment(Qt::AlignCenter);

        showState(State::Default);
    }

    void showState(State state)
    {
        clearLayout();

        switch (state) {
        case State::Default:
            buildDefaultWelcome();
            break;
        case State::AuthRequired:
            buildBannerState(
                QStringLiteral("\U0001F512"),
                tr("Sign In Required"),
                tr("Sign in with your GitHub account to use Copilot."),
                ChatTheme::WarningFg,
                State::AuthRequired);
            break;
        case State::RateLimited:
            buildBannerState(
                QStringLiteral("\u23F3"),
                tr("Rate Limit Reached"),
                tr("You've reached the rate limit for this model.\n"
                   "Try again in a moment, or switch to a different model."),
                ChatTheme::WarningFg,
                State::RateLimited);
            break;
        case State::Offline:
            buildBannerState(
                QStringLiteral("\u26A0"),
                tr("Network Unavailable"),
                tr("Check your internet connection and try again."),
                ChatTheme::ErrorFg,
                State::Offline);
            break;
        case State::NoProvider:
            buildBannerState(
                QStringLiteral("\u2699"),
                tr("No Provider Configured"),
                tr("Configure an AI provider to get started.\n"
                   "Copilot, Claude, and Codex plugins are supported."),
                ChatTheme::FgDimmed,
                State::NoProvider);
            break;
        }
    }

signals:
    void suggestionClicked(const QString &message);
    void signInRequested();
    void retryRequested();

private:
    void clearLayout()
    {
        while (auto *item = m_layout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }

    void buildDefaultWelcome()
    {
        // Icon
        auto *icon = new QLabel(this);
        icon->setText(QStringLiteral("\u2728"));
        icon->setStyleSheet(QStringLiteral("font-size:48px;"));
        icon->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(icon);

        // Title
        auto *title = new QLabel(tr("Exorcist AI Assistant"), this);
        title->setStyleSheet(
            QStringLiteral("color:%1; font-size:18px; font-weight:600;")
                .arg(ChatTheme::FgPrimary));
        title->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(title);

        // Subtitle
        auto *subtitle = new QLabel(
            tr("Ask questions about your code, generate edits,\n"
               "or let the agent work autonomously."),
            this);
        subtitle->setWordWrap(true);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet(
            QStringLiteral("color:%1; font-size:13px;").arg(ChatTheme::FgSecondary));
        m_layout->addWidget(subtitle);

        m_layout->addSpacing(16);

        // Suggestion chips
        auto *suggestionsLabel = new QLabel(tr("Try asking:"), this);
        suggestionsLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; font-weight:600;")
                .arg(ChatTheme::FgDimmed));
        suggestionsLabel->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(suggestionsLabel);

        const QStringList suggestions = {
            tr("Explain this codebase"),
            tr("Find and fix bugs in this file"),
            tr("Add unit tests for the selected code"),
            tr("Refactor the selected function"),
        };

        for (const auto &text : suggestions) {
            auto *btn = new QToolButton(this);
            btn->setText(text);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setStyleSheet(
                QStringLiteral("QToolButton {"
                              "  color:%1; background:%2; border:1px solid %3;"
                              "  border-radius:8px; padding:8px 16px;"
                              "  font-size:12px; text-align:left;"
                              "}"
                              "QToolButton:hover {"
                              "  background:%4; border-color:%5;"
                              "}")
                    .arg(ChatTheme::FgPrimary, ChatTheme::InputBg,
                         ChatTheme::Border, ChatTheme::PanelBg,
                         ChatTheme::FgDimmed));
            const auto msg = text;
            connect(btn, &QToolButton::clicked, this, [this, msg]() {
                emit suggestionClicked(msg);
            });
            m_layout->addWidget(btn);
        }

        m_layout->addStretch();
    }

    void buildBannerState(const QString &icon, const QString &title,
                          const QString &message, const char *accentColor,
                          State state = State::Default)
    {
        auto *iconLabel = new QLabel(this);
        iconLabel->setText(icon);
        iconLabel->setStyleSheet(QStringLiteral("font-size:48px;"));
        iconLabel->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(iconLabel);

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:16px; font-weight:600;")
                .arg(QLatin1String(accentColor)));
        titleLabel->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(titleLabel);

        auto *msgLabel = new QLabel(message, this);
        msgLabel->setWordWrap(true);
        msgLabel->setAlignment(Qt::AlignCenter);
        msgLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:13px;").arg(ChatTheme::FgSecondary));
        m_layout->addWidget(msgLabel);

        // Action button for relevant states
        if (state == State::AuthRequired) {
            m_layout->addSpacing(12);
            auto *signInBtn = new QToolButton(this);
            signInBtn->setText(tr("Sign In with GitHub"));
            signInBtn->setCursor(Qt::PointingHandCursor);
            signInBtn->setStyleSheet(
                QStringLiteral("QToolButton { color:%1; background:%2;"
                              " border:none; border-radius:6px;"
                              " padding:8px 20px; font-size:13px; font-weight:600; }"
                              "QToolButton:hover { background:%3; }")
                    .arg(ChatTheme::ButtonFg, ChatTheme::ButtonBg,
                         ChatTheme::ButtonHover));
            connect(signInBtn, &QToolButton::clicked, this, &ChatWelcomeWidget::signInRequested);
            m_layout->addWidget(signInBtn, 0, Qt::AlignCenter);
        } else if (state == State::Offline || state == State::RateLimited) {
            m_layout->addSpacing(12);
            auto *retryBtn = new QToolButton(this);
            retryBtn->setText(tr("Retry"));
            retryBtn->setCursor(Qt::PointingHandCursor);
            retryBtn->setStyleSheet(
                QStringLiteral("QToolButton { color:%1; background:%2;"
                              " border:1px solid %3; border-radius:6px;"
                              " padding:6px 16px; font-size:13px; }"
                              "QToolButton:hover { background:%4; }")
                    .arg(ChatTheme::FgPrimary, ChatTheme::InputBg,
                         ChatTheme::Border, ChatTheme::HoverBg));
            connect(retryBtn, &QToolButton::clicked, this, &ChatWelcomeWidget::retryRequested);
            m_layout->addWidget(retryBtn, 0, Qt::AlignCenter);
        }

        m_layout->addStretch();
    }

    QVBoxLayout *m_layout = nullptr;
};
