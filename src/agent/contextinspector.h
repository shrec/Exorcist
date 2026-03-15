#pragma once

#include <QStringList>
#include <QWidget>

class QJsonArray;
class QJsonObject;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QTableWidget;

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
    explicit ContextInspector(QWidget *parent = nullptr);

    void showContext(const QJsonArray &items);
    void showRawRequest(const QJsonObject &request);

private:
    QSplitter *m_splitter;
    QTableWidget *m_table;
    QPlainTextEdit *m_preview;
    QLabel *m_totalLabel;
    QStringList m_contents;
};
