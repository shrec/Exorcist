#pragma once

#include "../itool.h"
#include "../notebookstubs.h"

// ─────────────────────────────────────────────────────────────────────────────
// NotebookTools — agent tools for Jupyter notebook (.ipynb) interaction.
//
// Provides: notebookContext, executeCell, readCellOutput, createNotebook,
// editNotebookCells, getNotebookSummary.
// ─────────────────────────────────────────────────────────────────────────────

/// Tool: inject notebook cell contents as context
class NotebookContextTool : public ITool
{
public:
    explicit NotebookContextTool(NotebookManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    NotebookDocument loadNotebookFromFile(const QString &path) const;

    NotebookManager *m_mgr;
};

/// Tool: read cell output from a notebook
class ReadCellOutputTool : public ITool
{
public:
    explicit ReadCellOutputTool(NotebookManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    NotebookManager *m_mgr;
};

/// Tool: create a new .ipynb notebook
class CreateNotebookTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

/// Tool: edit notebook cells (add/modify/delete)
class EditNotebookCellsTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

/// Tool: get notebook summary (structure, cell count, types, outputs)
class GetNotebookSummaryTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};
