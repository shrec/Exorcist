#pragma once

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

#include "chatthemetokens.h"

// Helper: makes a QWidget clickable via mouse press event filter
class PillClickFilter : public QObject
{
    Q_OBJECT
public:
    PillClickFilter(QObject *parent, std::function<void()> callback)
        : QObject(parent), m_callback(std::move(callback)) {}

    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::MouseButtonRelease) {
            if (m_callback) m_callback();
            return true;
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    std::function<void()> m_callback;
};

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
        m_layout->setContentsMargins(24, 0, 24, 20);
        m_layout->setSpacing(10);
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
            buildDefaultWelcome(true);
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

    void buildDefaultWelcome(bool disabled = false)
    {
        m_layout->addStretch();

        // Sparkle icon (VS Code uses a ✦ sparkle)
        auto *icon = new QLabel(this);
        icon->setText(QStringLiteral("\u2726"));
        icon->setStyleSheet(QStringLiteral("font-size:32px; color:%1;")
            .arg(disabled ? ChatTheme::FgDimmed : ChatTheme::FgPrimary));
        icon->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(icon);

        // Title
        auto *title = new QLabel(disabled ? tr("No Provider Configured") : tr("Ask Copilot"), this);
        title->setStyleSheet(
            QStringLiteral("color:%1; font-size:15px; font-weight:600;")
                .arg(disabled ? ChatTheme::FgDimmed : ChatTheme::FgPrimary));
        title->setAlignment(Qt::AlignCenter);
        m_layout->addWidget(title);

        // Subtitle
        auto *subtitle = new QLabel(
            disabled
                ? tr("Configure an AI provider to get started.")
                : tr("Pick a suggestion or just start typing"),
            this);
        subtitle->setWordWrap(true);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px;").arg(ChatTheme::FgSecondary));
        m_layout->addWidget(subtitle);

        m_layout->addSpacing(12);

        // Suggestion chips (VS Code style pills).
        // prefix = slash command shown in accent color (or empty)
        // label  = description text
        // command = text sent when clicked
        struct Suggestion { QString prefix; QString label; QString command; };
        const QList<Suggestion> suggestions = {
            { QStringLiteral("/explain"), tr("Explain this codebase"),             QStringLiteral("/explain") },
            { QStringLiteral("/fix"),     tr("Find and fix bugs in this file"),    QStringLiteral("/fix") },
            { QStringLiteral("/tests"),   tr("Add unit tests for the selected code"),  QStringLiteral("/tests") },
            { {},                         tr("Refactor the selected function"),    QStringLiteral("Refactor the selected function") },
        };

        for (const auto &sug : suggestions) {
            auto *pill = new QWidget(this);
            pill->setAttribute(Qt::WA_Hover, true);
            pill->setCursor(Qt::PointingHandCursor);
            pill->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            pill->setFixedHeight(32);
            pill->setStyleSheet(
                QStringLiteral("QWidget {"
                              "  background:%1; border:1px solid %2;"
                              "  border-radius:16px; }"
                              "QWidget:hover { background:%3; border-color:%4; }")
                    .arg(ChatTheme::InputBg, ChatTheme::Border,
                         ChatTheme::HoverBg, ChatTheme::FgDimmed));

            auto *hl = new QHBoxLayout(pill);
            hl->setContentsMargins(14, 0, 14, 0);
            hl->setSpacing(6);

            if (!sug.prefix.isEmpty()) {
                auto *prefixLbl = new QLabel(sug.prefix, pill);
                prefixLbl->setStyleSheet(
                    QStringLiteral("color:%1; font-size:12px; font-weight:600;"
                                  " background:transparent; border:none;")
                        .arg(disabled ? ChatTheme::FgDimmed
                                      : ChatTheme::pick(ChatTheme::SlashPillFg,
                                                        ChatTheme::L_SlashPillFg)));
                hl->addWidget(prefixLbl);
            }

            auto *lbl = new QLabel(sug.label, pill);
            lbl->setStyleSheet(
                QStringLiteral("color:%1; font-size:12px; background:transparent; border:none;")
                    .arg(disabled ? ChatTheme::FgDimmed : ChatTheme::FgPrimary));
            hl->addWidget(lbl);
            hl->addStretch();

            if (!disabled) {
                const auto cmd = sug.command;
                // Make the whole pill clickable by installing an event filter
                pill->installEventFilter(new PillClickFilter(this, [this, cmd]() {
                    emit suggestionClicked(cmd);
                }));
            }
            pill->setEnabled(!disabled);
            m_layout->addWidget(pill);
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
