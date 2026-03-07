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

    QJsonObject toSummary() const {
        QJsonObject obj;
        obj[QStringLiteral("file")] = filePath;
        obj[QStringLiteral("kernel")] = kernelName;
        obj[QStringLiteral("cellCount")] = cells.size();

        int codeCells = 0, markdownCells = 0;
        for (const auto &c : cells) {
            if (c.type == NotebookCell::CellType::Code) ++codeCells;
            else if (c.type == NotebookCell::CellType::Markdown) ++markdownCells;
        }
        obj[QStringLiteral("codeCells")] = codeCells;
        obj[QStringLiteral("markdownCells")] = markdownCells;
        return obj;
    }
};

class NotebookManager : public QObject
{
    Q_OBJECT

public:
    explicit NotebookManager(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Check if a file is a notebook
    static bool isNotebook(const QString &filePath)
    {
        return filePath.endsWith(QStringLiteral(".ipynb"), Qt::CaseInsensitive);
    }

    /// Load a notebook (stub — returns empty)
    NotebookDocument loadNotebook(const QString &filePath)
    {
        Q_UNUSED(filePath)
        // Stub: real implementation will parse .ipynb JSON
        return {};
    }

    /// Get currently open notebook (stub)
    NotebookDocument currentNotebook() const { return m_current; }

    /// Get cell content for context injection
    QString cellContext(const NotebookDocument &doc, int cellIndex) const
    {
        if (cellIndex < 0 || cellIndex >= doc.cells.size())
            return {};
        const auto &cell = doc.cells[cellIndex];
        QString ctx;
        ctx += QStringLiteral("# Cell %1 (%2)\n")
                   .arg(cellIndex + 1)
                   .arg(cell.type == NotebookCell::CellType::Code
                            ? QStringLiteral("code")
                            : QStringLiteral("markdown"));
        ctx += cell.source;
        if (!cell.outputs.isEmpty()) {
            ctx += QStringLiteral("\n# Output:\n");
            ctx += cell.outputs.join('\n');
        }
        return ctx;
    }

    /// Get full notebook context (all cells)
    QString fullContext(const NotebookDocument &doc) const
    {
        QStringList parts;
        for (int i = 0; i < doc.cells.size(); ++i)
            parts << cellContext(doc, i);
        return parts.join(QStringLiteral("\n\n"));
    }

signals:
    void notebookOpened(const QString &filePath);
    void notebookClosed(const QString &filePath);
    void cellExecuted(int cellIndex, const QStringList &output);

private:
    NotebookDocument m_current;
};
