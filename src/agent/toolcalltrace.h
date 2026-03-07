#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// ToolCallTrace — detailed log of all tool invocations per turn.
//
// Shows tool name, arguments, result/error, and timing for each call.
// Used as a developer debugging panel.
// ─────────────────────────────────────────────────────────────────────────────

struct ToolCallRecord {
    int turnNumber = 0;
    QString toolName;
    QJsonObject arguments;
    QJsonObject result;
    bool success = false;
    QString error;
    qint64 durationMs = 0;
    QString timestamp;
};

class ToolCallTrace : public QWidget
{
    Q_OBJECT

public:
    explicit ToolCallTrace(QWidget *parent = nullptr) : QWidget(parent)
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

    void addRecord(const ToolCallRecord &record)
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

    void clear()
    {
        m_tree->clear();
        m_records.clear();
        m_detail->clear();
        m_countLabel->clear();
    }

    // Export as JSON
    QJsonArray toJson() const
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

    QList<ToolCallRecord> records() const { return m_records; }

private:
    QTreeWidget *m_tree;
    QPlainTextEdit *m_detail;
    QLabel *m_countLabel;
    QList<ToolCallRecord> m_records;
};
