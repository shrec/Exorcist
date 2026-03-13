#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// NotebookStubs — stub interfaces for Jupyter notebook support.
//
// These are placeholder types and a minimal manager so that the agent
// system can reference notebook concepts (cells, outputs, execution)
// without requiring an actual notebook renderer. The real implementation
// will come in a future phase.
// ─────────────────────────────────────────────────────────────────────────────

struct NotebookCell {
    enum class CellType { Code, Markdown, Raw };

    int index = 0;
    CellType type = CellType::Code;
    QString language;       // e.g. "python"
    QString source;         // cell content
    QStringList outputs;    // execution output lines
    bool executed = false;
    int executionCount = 0;
};

struct NotebookDocument {
    QString filePath;
    QString kernelName;     // e.g. "python3"
    QList<NotebookCell> cells;

    bool isValid() const { return !filePath.isEmpty(); }
    QJsonObject toSummary() const;
};

class NotebookManager : public QObject
{
    Q_OBJECT

public:
    explicit NotebookManager(QObject *parent = nullptr);

    static bool isNotebook(const QString &filePath);
    NotebookDocument loadNotebook(const QString &filePath);
    NotebookDocument currentNotebook() const { return m_current; }
    QString cellContext(const NotebookDocument &doc, int cellIndex) const;
    QString fullContext(const NotebookDocument &doc) const;

signals:
    void notebookOpened(const QString &filePath);
    void notebookClosed(const QString &filePath);
    void cellExecuted(int cellIndex, const QStringList &output);

private:
    NotebookDocument m_current;
};
