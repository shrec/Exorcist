#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTableWidget;

// ─────────────────────────────────────────────────────────────────────────────
// PromptArchiveExporter — export all prompts/requests as viewable archive.
//
// Shows a table of all recorded prompts (system + user messages) per turn
// and allows exporting them as a JSON file.
// ─────────────────────────────────────────────────────────────────────────────

struct PromptRecord {
    int turn = 0;
    QString role;    // "system", "user", "assistant"
    QString content;
    int tokens = 0;
    QString model;
    QString timestamp;
};

class PromptArchiveExporter : public QWidget
{
    Q_OBJECT

public:
    explicit PromptArchiveExporter(QWidget *parent = nullptr);

    void addRecord(const PromptRecord &record);
    QJsonArray toJson() const;
    void clear();

signals:
    void exportRequested();

private:
    QTableWidget *m_table;
    QPlainTextEdit *m_preview;
    QLabel *m_countLabel;
    QList<PromptRecord> m_records;
};
