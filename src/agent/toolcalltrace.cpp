#include "toolcalltrace.h"

#include <QFont>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ToolCallTrace::ToolCallTrace(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *title = new QLabel(tr("Tool Call Trace"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Turn"), tr("Tool"), tr("Status"), tr("Duration")});
    m_tree->setColumnWidth(0, 50);
    m_tree->setColumnWidth(1, 150);
    m_tree->setColumnWidth(2, 60);
    m_tree->setColumnWidth(3, 80);
    layout->addWidget(m_tree);

    m_detail = new QPlainTextEdit(this);
    m_detail->setReadOnly(true);
    m_detail->setFont(QFont(QStringLiteral("Consolas"), 10));
    m_detail->setMaximumHeight(180);
    layout->addWidget(m_detail);

    m_countLabel = new QLabel(this);
    layout->addWidget(m_countLabel);

    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (!current) return;
        const int idx = current->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_records.size()) {
            const auto &r = m_records[idx];
            QJsonObject detail;
            detail[QStringLiteral("tool")] = r.toolName;
            detail[QStringLiteral("args")] = r.arguments;
            detail[QStringLiteral("result")] = r.result;
            if (!r.error.isEmpty())
                detail[QStringLiteral("error")] = r.error;
            m_detail->setPlainText(
                QString::fromUtf8(QJsonDocument(detail).toJson(QJsonDocument::Indented)));
        }
    });
}

void ToolCallTrace::addRecord(const ToolCallRecord &record)
{
    m_records.append(record);
    auto *item = new QTreeWidgetItem;
    item->setText(0, QString::number(record.turnNumber));
    item->setText(1, record.toolName);
    item->setText(2, record.success ? QStringLiteral("\u2705") : QStringLiteral("\u274C"));
    item->setText(3, QStringLiteral("%1ms").arg(record.durationMs));
    item->setData(0, Qt::UserRole, m_records.size() - 1);
    m_tree->addTopLevelItem(item);
    m_countLabel->setText(tr("%1 tool calls").arg(m_records.size()));
}

void ToolCallTrace::clear()
{
    m_tree->clear();
    m_records.clear();
    m_detail->clear();
    m_countLabel->clear();
}

QJsonArray ToolCallTrace::toJson() const
{
    QJsonArray arr;
    for (const auto &r : m_records) {
        QJsonObject obj;
        obj[QStringLiteral("turn")] = r.turnNumber;
        obj[QStringLiteral("tool")] = r.toolName;
        obj[QStringLiteral("args")] = r.arguments;
        obj[QStringLiteral("result")] = r.result;
        obj[QStringLiteral("success")] = r.success;
        obj[QStringLiteral("error")] = r.error;
        obj[QStringLiteral("durationMs")] = r.durationMs;
        obj[QStringLiteral("timestamp")] = r.timestamp;
        arr.append(obj);
    }
    return arr;
}
