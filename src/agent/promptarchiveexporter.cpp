#include "promptarchiveexporter.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

PromptArchiveExporter::PromptArchiveExporter(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *title = new QLabel(tr("Prompt Archive"), this);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(title);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Turn"), tr("Role"), tr("Tokens"), tr("Model"), tr("Time")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(m_table);

    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setFont(QFont(QStringLiteral("Consolas"), 10));
    splitter->addWidget(m_preview);

    layout->addWidget(splitter);

    auto *btnRow = new QHBoxLayout;
    auto *exportBtn = new QPushButton(tr("Export JSON"), this);
    connect(exportBtn, &QPushButton::clicked, this, &PromptArchiveExporter::exportRequested);
    btnRow->addWidget(exportBtn);

    auto *clearBtn = new QPushButton(tr("Clear"), this);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_records.clear();
        m_table->setRowCount(0);
        m_preview->clear();
    });
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();

    m_countLabel = new QLabel(this);
    btnRow->addWidget(m_countLabel);
    layout->addLayout(btnRow);

    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
        if (row >= 0 && row < m_records.size())
            m_preview->setPlainText(m_records[row].content);
    });
}

void PromptArchiveExporter::addRecord(const PromptRecord &record)
{
    m_records.append(record);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(record.turn)));
    m_table->setItem(row, 1, new QTableWidgetItem(record.role));
    m_table->setItem(row, 2, new QTableWidgetItem(QString::number(record.tokens)));
    m_table->setItem(row, 3, new QTableWidgetItem(record.model));
    m_table->setItem(row, 4, new QTableWidgetItem(record.timestamp));
    m_countLabel->setText(tr("%1 prompts").arg(m_records.size()));
}

QJsonArray PromptArchiveExporter::toJson() const
{
    QJsonArray arr;
    for (const auto &r : m_records) {
        QJsonObject obj;
        obj[QStringLiteral("turn")] = r.turn;
        obj[QStringLiteral("role")] = r.role;
        obj[QStringLiteral("content")] = r.content;
        obj[QStringLiteral("tokens")] = r.tokens;
        obj[QStringLiteral("model")] = r.model;
        obj[QStringLiteral("timestamp")] = r.timestamp;
        arr.append(obj);
    }
    return arr;
}

void PromptArchiveExporter::clear()
{
    m_records.clear();
    m_table->setRowCount(0);
    m_preview->clear();
}
