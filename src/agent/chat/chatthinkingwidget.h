#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

// ── ChatThinkingWidget ────────────────────────────────────────────────────────
//
// Renders a "Thinking..." block with collapsible content.
// Supports live streaming of thinking tokens and collapse/expand toggle.

class ChatThinkingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatThinkingWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setStyleSheet(
            QStringLiteral("ChatThinkingWidget {"
                          " background:%1; border-left:3px solid %2;"
                          " border-radius:2px; margin:4px 0; }")
                .arg(ChatTheme::ThinkingBg, ChatTheme::ThinkingBorder));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(4);

        // Header row with toggle
        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(4);

        m_headerLabel = new QLabel(this);
        m_headerLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;").arg(ChatTheme::ThinkingSummary));
        m_headerLabel->setText(tr("\U0001F4A1 Thinking\u2026"));

        m_toggleBtn = new QToolButton(this);
        m_toggleBtn->setText(QStringLiteral("\u25BC"));
        m_toggleBtn->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:transparent; border:none;"
                          " font-size:10px; padding:2px; }")
                .arg(ChatTheme::ThinkingSummary));
        m_toggleBtn->setCheckable(true);
        m_toggleBtn->setChecked(true);
        connect(m_toggleBtn, &QToolButton::toggled, this, [this](bool expanded) {
            m_contentLabel->setVisible(expanded);
            m_toggleBtn->setText(expanded ? QStringLiteral("\u25BC") : QStringLiteral("\u25B6"));
            updateGeometry();
        });

        headerRow->addWidget(m_headerLabel, 1);
        headerRow->addWidget(m_toggleBtn);
        layout->addLayout(headerRow);

        // Content area (scrollable, max 200px)
        m_contentLabel = new QLabel(this);
        m_contentLabel->setWordWrap(true);
        m_contentLabel->setTextFormat(Qt::RichText);
        m_contentLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; margin-top:4px;"
                          " white-space:pre-wrap;")
                .arg(ChatTheme::ThinkingFg));
        auto *contentScroll = new QScrollArea(this);
        contentScroll->setWidgetResizable(true);
        contentScroll->setFrameShape(QFrame::NoFrame);
        contentScroll->setMaximumHeight(200);
        contentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        contentScroll->setStyleSheet(
            QStringLiteral("QScrollArea { background:transparent; border:none; }"));
        contentScroll->setWidget(m_contentLabel);
        layout->addWidget(contentScroll);
    }

    void setThinkingText(const QString &text, bool streaming = false)
    {
        m_rawText = text;
        const QString safe = text.toHtmlEscaped()
            .replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        if (streaming) {
            m_contentLabel->setText(
                safe + QStringLiteral("<span style='color:%1;'>\u2588</span>")
                    .arg(ChatTheme::AccentPurple));
            m_headerLabel->setText(tr("\U0001F4A1 Thinking\u2026"));
        } else {
            m_contentLabel->setText(safe);
            m_headerLabel->setText(tr("\U0001F4A1 Thinking"));
        }
    }

    void appendDelta(const QString &delta)
    {
        m_rawText += delta;
        setThinkingText(m_rawText, true);
    }

    void finalize()
    {
        setThinkingText(m_rawText, false);
    }

    void setCollapsed(bool collapsed)
    {
        m_toggleBtn->setChecked(!collapsed);
    }

    QString rawText() const { return m_rawText; }

private:
    QLabel      *m_headerLabel  = nullptr;
    QLabel      *m_contentLabel = nullptr;
    QToolButton *m_toggleBtn    = nullptr;
    QString      m_rawText;
};
