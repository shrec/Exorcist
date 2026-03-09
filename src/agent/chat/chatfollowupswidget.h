#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

// ── ChatFollowupsWidget ──────────────────────────────────────────────────────
//
// Renders a row of clickable followup suggestion chips.
// Each chip is a rounded-pill button.  Matches VS Code's followup buttons
// shown after Copilot finishes a response.

class ChatFollowupsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatFollowupsWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 6, 0, 2);
        m_layout->setSpacing(2);
    }

    void setFollowups(const QVector<ChatContentPart::FollowupItem> &items)
    {
        // Clear existing buttons
        while (auto *item = m_layout->takeAt(0)) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }

        for (const auto &fu : items) {
            auto *btn = new QToolButton(this);
            btn->setText(fu.label.isEmpty() ? fu.message : fu.label);
            btn->setToolTip(fu.tooltip.isEmpty() ? fu.message : fu.tooltip);
            btn->setStyleSheet(chipStyle());
            btn->setCursor(Qt::PointingHandCursor);

            const auto msg = fu.message;
            connect(btn, &QToolButton::clicked, this, [this, msg]() {
                emit followupClicked(msg);
            });
            m_layout->addWidget(btn);
        }
    }

signals:
    void followupClicked(const QString &message);

private:
    static QString chipStyle()
    {
        return QStringLiteral(
            "QToolButton {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  padding: 2px 0px;"
            "  font-size: 12px;"
            "  text-align: left;"
            "}"
            "QToolButton:hover {"
            "  color: %2;"
            "  text-decoration: underline;"
            "}")
            .arg(ChatTheme::LinkBlue, ChatTheme::AccentBlue);
    }

    QVBoxLayout *m_layout = nullptr;
};
