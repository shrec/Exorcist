#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

// ── ChatToolInvocationWidget ──────────────────────────────────────────────────
//
// Renders a tool invocation card matching VS Code's Working panel style.
// Shows:
//   - Icon + friendly title + state indicator
//   - Present-tense or past-tense message
//   - Collapsible IO panel (input args / output result)
//   - Confirmation buttons (when state == ConfirmationNeeded)
//   - Spinner while active
//
// State transitions are animated.

class ChatToolInvocationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatToolInvocationWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 3, 0, 3);
        layout->setSpacing(0);

        m_card = new QWidget(this);
        m_card->setStyleSheet(
            QStringLiteral("background:%1; border-radius:2px; padding:5px 10px;"
                          " border-left:3px solid %2;")
                .arg(ChatTheme::ToolBg, ChatTheme::ToolBorderDefault));

        auto *cardLayout = new QVBoxLayout(m_card);
        cardLayout->setContentsMargins(10, 5, 10, 5);
        cardLayout->setSpacing(2);

        // Title row: icon + name + state badge
        auto *titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(6);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px;").arg(ChatTheme::FgPrimary));
        m_iconLabel->setFixedWidth(16);

        m_titleLabel = new QLabel(this);
        m_titleLabel->setStyleSheet(
            QStringLiteral("color:%1; font-weight:600; font-size:12px;")
                .arg(ChatTheme::FgPrimary));

        m_stateBadge = new QLabel(this);
        m_stateBadge->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;").arg(ChatTheme::FgDimmed));

        m_ioToggle = new QToolButton(this);
        m_ioToggle->setText(QStringLiteral("\u25B6"));
        m_ioToggle->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:transparent;"
                          " border:none; font-size:10px; padding:2px; }")
                .arg(ChatTheme::FgDimmed));
        m_ioToggle->setCheckable(true);
        connect(m_ioToggle, &QToolButton::toggled, this, [this](bool on) {
            m_ioPanel->setVisible(on);
            m_ioToggle->setText(on ? QStringLiteral("\u25BC") : QStringLiteral("\u25B6"));
            updateGeometry();
        });

        titleRow->addWidget(m_iconLabel);
        titleRow->addWidget(m_titleLabel, 1);
        titleRow->addWidget(m_stateBadge);
        titleRow->addWidget(m_ioToggle);
        cardLayout->addLayout(titleRow);

        // Description
        m_descLabel = new QLabel(this);
        m_descLabel->setWordWrap(true);
        m_descLabel->setTextFormat(Qt::RichText);
        m_descLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px;").arg(ChatTheme::FgSecondary));
        cardLayout->addWidget(m_descLabel);

        // Collapsible IO panel
        m_ioPanel = new QWidget(this);
        m_ioPanel->hide();
        auto *ioLayout = new QVBoxLayout(m_ioPanel);
        ioLayout->setContentsMargins(0, 4, 0, 0);
        ioLayout->setSpacing(2);

        m_inputLabel = new QLabel(this);
        m_inputLabel->setWordWrap(true);
        m_inputLabel->setTextFormat(Qt::RichText);
        m_inputLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px; font-family:%2;"
                          " background:%3; padding:4px 6px; border-radius:2px;")
                .arg(ChatTheme::FgDimmed, ChatTheme::MonoFamily, ChatTheme::CodeBg));

        m_outputLabel = new QLabel(this);
        m_outputLabel->setWordWrap(true);
        m_outputLabel->setTextFormat(Qt::RichText);
        m_outputLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px; font-family:%2;"
                          " background:%3; padding:4px 6px; border-radius:2px;")
                .arg(ChatTheme::FgDimmed, ChatTheme::MonoFamily, ChatTheme::CodeBg));

        ioLayout->addWidget(m_inputLabel);
        ioLayout->addWidget(m_outputLabel);
        cardLayout->addWidget(m_ioPanel);

        // Confirmation buttons (hidden unless state == ConfirmationNeeded)
        m_confirmRow = new QWidget(this);
        m_confirmRow->hide();
        auto *confirmLayout = new QHBoxLayout(m_confirmRow);
        confirmLayout->setContentsMargins(0, 4, 0, 0);
        confirmLayout->setSpacing(6);

        m_allowBtn = new QToolButton(this);
        m_allowBtn->setText(tr("Allow"));
        m_allowBtn->setStyleSheet(
            QStringLiteral("QToolButton { background:%1; color:%2; padding:3px 10px;"
                          " border-radius:4px; font-size:12px; border:none; }"
                          "QToolButton:hover { background:%3; }")
                .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg, ChatTheme::ButtonHover));
        connect(m_allowBtn, &QToolButton::clicked, this, [this]() {
            emit confirmed(m_callId, true);
        });

        m_denyBtn = new QToolButton(this);
        m_denyBtn->setText(tr("Deny"));
        m_denyBtn->setStyleSheet(
            QStringLiteral("QToolButton { background:%1; color:%2; padding:3px 10px;"
                          " border-radius:4px; font-size:12px; border:none; }"
                          "QToolButton:hover { background:%3; }")
                .arg(ChatTheme::SecondaryBtnBg, ChatTheme::SecondaryBtnFg,
                     ChatTheme::SecondaryBtnHover));
        connect(m_denyBtn, &QToolButton::clicked, this, [this]() {
            emit confirmed(m_callId, false);
        });

        confirmLayout->addWidget(m_allowBtn);
        confirmLayout->addWidget(m_denyBtn);
        confirmLayout->addStretch();
        cardLayout->addWidget(m_confirmRow);

        layout->addWidget(m_card);
    }

    void setToolData(const ChatContentPart &part)
    {
        m_callId = part.toolCallId;
        m_titleLabel->setText(part.toolFriendlyTitle.isEmpty()
            ? part.toolName : part.toolFriendlyTitle);
        m_iconLabel->setText(iconForTool(part.toolName));

        if (!part.toolInput.isEmpty())
            m_inputLabel->setText(
                QStringLiteral("<b>Input:</b> %1").arg(part.toolInput.toHtmlEscaped().left(500)));
        if (!part.toolOutput.isEmpty())
            m_outputLabel->setText(
                QStringLiteral("<b>Output:</b> %1").arg(part.toolOutput.toHtmlEscaped().left(500)));

        updateState(part);
    }

    void updateState(const ChatContentPart &part)
    {
        const auto state = part.toolState;
        QString borderColor;
        QString stateText;

        // Stop spinner if running
        if (m_spinTimer) {
            m_spinTimer->stop();
            m_spinTimer->deleteLater();
            m_spinTimer = nullptr;
        }

        switch (state) {
        case ChatContentPart::ToolState::Queued:
            borderColor = ChatTheme::ToolBorderDefault;
            stateText = QStringLiteral("\u25CB queued");
            m_descLabel->setText(part.toolInvocationMsg);
            break;
        case ChatContentPart::ToolState::Streaming:
            borderColor = ChatTheme::ToolBorderActive;
            stateText = spinnerFrame() + QStringLiteral(" running\u2026");
            m_descLabel->setText(part.toolInvocationMsg);
            startSpinner();
            break;
        case ChatContentPart::ToolState::ConfirmationNeeded:
            borderColor = ChatTheme::WarningFg;
            stateText = QStringLiteral("\u26A0 confirmation needed");
            m_descLabel->setText(part.toolInvocationMsg);
            m_confirmRow->show();
            break;
        case ChatContentPart::ToolState::CompleteSuccess:
            borderColor = ChatTheme::ToolBorderOk;
            stateText = QStringLiteral("\u2713");
            m_descLabel->setText(part.toolPastTenseMsg.isEmpty()
                ? part.toolInvocationMsg : part.toolPastTenseMsg);
            m_confirmRow->hide();
            break;
        case ChatContentPart::ToolState::CompleteError:
            borderColor = ChatTheme::ToolBorderFail;
            stateText = QStringLiteral("\u2717 failed");
            m_descLabel->setText(part.toolOutput.isEmpty()
                ? part.toolInvocationMsg : part.toolOutput);
            m_confirmRow->hide();
            break;
        }

        m_card->setStyleSheet(
            QStringLiteral("background:%1; border-radius:2px; padding:5px 10px;"
                          " border-left:3px solid %2;")
                .arg(ChatTheme::ToolBg, borderColor));
        m_stateBadge->setText(stateText);

        if (!part.toolOutput.isEmpty())
            m_outputLabel->setText(
                QStringLiteral("<b>Output:</b> %1")
                    .arg(part.toolOutput.toHtmlEscaped().left(500)));
    }

signals:
    void confirmed(const QString &callId, bool allowed);

private:
    static QString iconForTool(const QString &toolName)
    {
        if (toolName.contains(QLatin1String("grep")) || toolName.contains(QLatin1String("search")))
            return QStringLiteral("Q");
        if (toolName.contains(QLatin1String("read")))
            return QStringLiteral("\u2630");
        if (toolName.contains(QLatin1String("list")))
            return QStringLiteral("\u2630");
        if (toolName.contains(QLatin1String("create")))
            return QStringLiteral("+");
        if (toolName.contains(QLatin1String("replace")) || toolName.contains(QLatin1String("edit")))
            return QStringLiteral("\u270E");
        if (toolName.contains(QLatin1String("terminal")) || toolName.contains(QLatin1String("run")))
            return QStringLiteral(">");
        if (toolName.contains(QLatin1String("think")))
            return QStringLiteral("\U0001F4A1");
        return QStringLiteral("\u25B6");
    }

    QWidget     *m_card        = nullptr;
    QLabel      *m_iconLabel   = nullptr;
    QLabel      *m_titleLabel  = nullptr;
    QLabel      *m_stateBadge  = nullptr;
    QLabel      *m_descLabel   = nullptr;
    QToolButton *m_ioToggle    = nullptr;
    QWidget     *m_ioPanel     = nullptr;
    QLabel      *m_inputLabel  = nullptr;
    QLabel      *m_outputLabel = nullptr;
    QWidget     *m_confirmRow  = nullptr;
    QToolButton *m_allowBtn    = nullptr;
    QToolButton *m_denyBtn     = nullptr;
    QString      m_callId;
    QTimer      *m_spinTimer   = nullptr;
    int          m_spinIndex   = 0;

    void startSpinner()
    {
        m_spinIndex = 0;
        m_spinTimer = new QTimer(this);
        connect(m_spinTimer, &QTimer::timeout, this, [this]() {
            m_spinIndex = (m_spinIndex + 1) % 4;
            static const QString frames[] = {
                QStringLiteral("\u25DC"), QStringLiteral("\u25DD"),
                QStringLiteral("\u25DE"), QStringLiteral("\u25DF")
            };
            m_stateBadge->setText(frames[m_spinIndex] + QStringLiteral(" running\u2026"));
        });
        m_spinTimer->start(150);
    }

    static QString spinnerFrame()
    {
        return QStringLiteral("\u25DC");
    }
};
