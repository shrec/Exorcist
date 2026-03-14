#pragma once

#include "../itool.h"

// ── SemanticSearchTool ────────────────────────────────────────────────────────
// A natural-language search tool for the workspace. It splits the query into
// keywords and scores files by how many distinct keywords match in their
// filename, path, or content. Returns the most relevant snippets sorted by
// relevance score.

class SemanticSearchTool : public ITool
{
public:
    explicit SemanticSearchTool(const QString &workspaceRoot = {});

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};
