#include "contexteditor.h"

ContextEditor::ContextEditor(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
}

void ContextEditor::buildUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 4, 6, 4);

    // Header
    auto *header = new QHBoxLayout;
    m_titleLabel = new QLabel(tr("Context"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    header->addWidget(m_titleLabel);
    header->addStretch();
    m_tokenLabel = new QLabel(this);
    header->addWidget(m_tokenLabel);
    m_layout->addLayout(header);

    // List
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_layout->addWidget(m_listWidget);
}

void ContextEditor::setItems(const QList<EditableContextItem> &items)
{
    m_items = items;
    refresh();
}

QList<EditableContextItem> ContextEditor::includedItems() const
{
    QList<EditableContextItem> result;
    for (const auto &item : m_items) {
        if (item.included)
            result.append(item);
    }
    return result;
}

int ContextEditor::totalTokens() const
{
    int total = 0;
    for (const auto &item : m_items) {
        if (item.included)
            total += item.tokens;
    }
    return total;
}

void ContextEditor::refresh()
{
    m_listWidget->clear();

    for (int i = 0; i < m_items.size(); ++i) {
        const auto &item = m_items[i];

        // Create a widget for this item
        auto *widget = new QWidget;
        auto *row = new QHBoxLayout(widget);
        row->setContentsMargins(2, 1, 2, 1);

        // Checkbox for include/exclude
        auto *cb = new QCheckBox(widget);
        cb->setChecked(item.included);
        connect(cb, &QCheckBox::toggled, this, [this, i](bool checked) {
            if (i < m_items.size()) {
                m_items[i].included = checked;
                m_tokenLabel->setText(tr("%1 tokens").arg(totalTokens()));
                emit itemsChanged();
            }
        });
        row->addWidget(cb);

        // Type icon
        QString icon;
        if (item.type == QStringLiteral("file")) icon = QStringLiteral("\U0001F4C4 ");
        else if (item.type == QStringLiteral("selection")) icon = QStringLiteral("\u2702 ");
        else if (item.type == QStringLiteral("diagnostics")) icon = QStringLiteral("\u26A0 ");
        else if (item.type == QStringLiteral("terminal")) icon = QStringLiteral("> ");
        else if (item.type == QStringLiteral("git")) icon = QStringLiteral("\U0001F500 ");

        auto *label = new QLabel(icon + item.label, widget);
        label->setToolTip(item.content.left(500));
        row->addWidget(label, 1);

        // Token count
        auto *tokenLabel = new QLabel(QString::number(item.tokens), widget);
        tokenLabel->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
        row->addWidget(tokenLabel);

        // Pin button
        auto *pinBtn = new QPushButton(item.pinned ? QStringLiteral("\U0001F4CC") : QStringLiteral("\u2022"), widget);
        pinBtn->setFixedSize(20, 20);
        pinBtn->setFlat(true);
        pinBtn->setToolTip(item.pinned ? tr("Unpin") : tr("Pin"));
        connect(pinBtn, &QPushButton::clicked, this, [this, i] {
            if (i < m_items.size()) {
                m_items[i].pinned = !m_items[i].pinned;
                emit itemPinToggled(m_items[i].key, m_items[i].pinned);
                refresh();
            }
        });
        row->addWidget(pinBtn);

        // Remove button
        auto *removeBtn = new QPushButton(QStringLiteral("\u2715"), widget);
        removeBtn->setFixedSize(20, 20);
        removeBtn->setFlat(true);
        removeBtn->setToolTip(tr("Remove"));
        connect(removeBtn, &QPushButton::clicked, this, [this, i] {
            if (i < m_items.size()) {
                const QString key = m_items[i].key;
                m_items.removeAt(i);
                emit itemRemoved(key);
                refresh();
            }
        });
        row->addWidget(removeBtn);

        auto *listItem = new QListWidgetItem;
        listItem->setSizeHint(widget->sizeHint());
        m_listWidget->addItem(listItem);
        m_listWidget->setItemWidget(listItem, widget);
    }

    m_tokenLabel->setText(tr("%1 tokens").arg(totalTokens()));
}
