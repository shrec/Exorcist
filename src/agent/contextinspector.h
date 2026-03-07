#pragma once

#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// ContextInspector — shows what context was sent with the last request.
//
// Displays context items with their token counts, types, and content.
// Used as a debug/developer tool.
// ─────────────────────────────────────────────────────────────────────────────

class ContextInspector : public QWidget
{
    Q_OBJECT

public:
    explicit ContextInspector(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        auto *title = new QLabel(tr("Context Inspector"), this);
        title->setStyleSheet(QStringLiteral("font-weight: bold;"));
        layout->addWidget(title);

        m_splitter = new QSplitter(Qt::Vertical, this);

        // Context items table
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({tr("Type"), tr("Label"), tr("Tokens"), tr("Pinned")});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_splitter->addWidget(m_table);

        // Content preview
        m_preview = new QPlainTextEdit(this);
        m_preview->setReadOnly(true);
        m_preview->setFont(QFont(QStringLiteral("Consolas"), 10));
        m_preview->setMaximumHeight(200);
        m_splitter->addWidget(m_preview);

        layout->addWidget(m_splitter);

        m_totalLabel = new QLabel(this);
        layout->addWidget(m_totalLabel);

        connect(m_table, &QTableWidget::currentCellChanged, this,
                [this](int row, int /*col*/, int /*prev*/, int /*prevcol*/) {
            if (row >= 0 && row < m_contents.size())
                m_preview->setPlainText(m_contents[row]);
        });
    }

    // Show context from a JSON representation
    void showContext(const QJsonArray &items)
    {
        m_table->setRowCount(0);
        m_contents.clear();
        int totalTokens = 0;

        for (const auto &item : items) {
            const QJsonObject obj = item.toObject();
            const int row = m_table->rowCount();
            m_table->insertRow(row);

            const QString type = obj.value(QStringLiteral("type")).toString();
            const QString label = obj.value(QStringLiteral("label")).toString();
            const QString content = obj.value(QStringLiteral("content")).toString();
            const int tokens = content.size() / 4;
            const bool pinned = obj.value(QStringLiteral("pinned")).toBool();

            m_table->setItem(row, 0, new QTableWidgetItem(type));
            m_table->setItem(row, 1, new QTableWidgetItem(label));
            m_table->setItem(row, 2, new QTableWidgetItem(QString::number(tokens)));
            m_table->setItem(row, 3, new QTableWidgetItem(pinned ? QStringLiteral("\U0001F4CC") : QString()));
            m_contents.append(content);
            totalTokens += tokens;
        }

        m_totalLabel->setText(tr("Total: %1 items, ~%2 tokens")
            .arg(items.size()).arg(totalTokens));
    }

    // Show raw JSON of last request
    void showRawRequest(const QJsonObject &request)
    {
        m_preview->setPlainText(
            QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Indented)));
    }

private:
    QSplitter *m_splitter;
    QTableWidget *m_table;
    QPlainTextEdit *m_preview;
    QLabel *m_totalLabel;
    QStringList m_contents;
};
