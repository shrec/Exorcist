#include "promptfilepicker.h"

#include <QListWidget>
#include <QVBoxLayout>

PromptFilePicker::PromptFilePicker(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setMinimumWidth(350);
    setMaximumHeight(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid palette(mid); border-radius: 4px; }"
        "QListWidget::item { padding: 6px; }"
        "QListWidget::item:hover { background: palette(highlight); color: palette(highlighted-text); }"));
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const int idx = m_list->row(item);
        if (idx >= 0 && idx < m_entries.size()) {
            emit promptSelected(m_entries[idx]);
            hide();
        }
    });
}

void PromptFilePicker::setEntries(const QList<PromptFileEntry> &entries)
{
    m_entries = entries;
    m_list->clear();
    for (const auto &e : entries) {
        const QString text = e.description.isEmpty()
            ? e.name
            : QStringLiteral("%1 — %2").arg(e.name, e.description);
        auto *item = new QListWidgetItem(text);
        item->setToolTip(e.filePath);
        m_list->addItem(item);
    }
}

void PromptFilePicker::showBelow(QWidget *anchor)
{
    if (!anchor) return;
    const QPoint pos = anchor->mapToGlobal(QPoint(0, -height()));
    move(pos);
    show();
    m_list->setFocus();
}
