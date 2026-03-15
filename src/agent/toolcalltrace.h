#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTreeWidget;

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
    explicit ToolCallTrace(QWidget *parent = nullptr);

    void addRecord(const ToolCallRecord &record);
    void clear();
    QJsonArray toJson() const;
    QList<ToolCallRecord> records() const { return m_records; }

private:
    QTreeWidget *m_tree;
    QPlainTextEdit *m_detail;
    QLabel *m_countLabel;
    QList<ToolCallRecord> m_records;
};
