#pragma once

#include "../itool.h"

// ── SearchWorkspaceTool ───────────────────────────────────────────────────────
// Searches for text patterns across workspace files.
// This is a synchronous grep-like search (not using SearchService threads
// for simplicity in tool context).

class SearchWorkspaceTool : public ITool
{
public:
    explicit SearchWorkspaceTool(const QString &workspaceRoot = {});

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};
