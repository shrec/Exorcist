#include "notebookstubs.h"

// ── NotebookDocument ──────────────────────────────────────────────────────────

QJsonObject NotebookDocument::toSummary() const
{
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

// ── NotebookManager ──────────────────────────────────────────────────────────

NotebookManager::NotebookManager(QObject *parent)
    : QObject(parent)
{}

bool NotebookManager::isNotebook(const QString &filePath)
{
    return filePath.endsWith(QStringLiteral(".ipynb"), Qt::CaseInsensitive);
}

NotebookDocument NotebookManager::loadNotebook(const QString &filePath)
{
    Q_UNUSED(filePath)
    // Stub: real implementation will parse .ipynb JSON
    return {};
}

QString NotebookManager::cellContext(const NotebookDocument &doc, int cellIndex) const
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

QString NotebookManager::fullContext(const NotebookDocument &doc) const
{
    QStringList parts;
    for (int i = 0; i < doc.cells.size(); ++i)
        parts << cellContext(doc, i);
    return parts.join(QStringLiteral("\n\n"));
}
