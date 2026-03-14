#pragma once

#include "../itool.h"

#include <functional>

// ── DiffTool ─────────────────────────────────────────────────────────────────
// Generates and shows file diffs. Can compare:
//   - Two files
//   - A file's current vs. proposed content
//   - Two arbitrary text blocks
// Optionally shows the diff in the IDE's side-by-side diff viewer.

class DiffTool : public ITool
{
public:
    // Callback to display side-by-side diff in the IDE
    using DiffViewer = std::function<bool(const QString &left, const QString &right,
                                          const QString &leftTitle, const QString &rightTitle)>;

    explicit DiffTool(DiffViewer viewer = {});

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString resolvePath(const QString &raw) const;
    static QString readFile(const QString &path);
    static QString generateUnifiedDiff(const QString &left, const QString &right,
                                        const QString &leftTitle, const QString &rightTitle);

    DiffViewer m_viewer;
    QString    m_workspaceRoot;
};
