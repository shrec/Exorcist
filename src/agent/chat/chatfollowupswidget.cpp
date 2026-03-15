#include "chatfollowupswidget.h"

#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

// ── Constructor ───────────────────────────────────────────────────────────────

ChatFollowupsWidget::ChatFollowupsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 6, 0, 2);
    m_layout->setSpacing(2);
}

// ── Public methods ────────────────────────────────────────────────────────────

void ChatFollowupsWidget::setFollowups(const QVector<ChatContentPart::FollowupItem> &items)
{
    // Clear existing buttons
    while (auto *item = m_layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        auto guard = std::unique_ptr<QLayoutItem>(item);
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

// ── Private helpers ───────────────────────────────────────────────────────────

QString ChatFollowupsWidget::chipStyle()
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
