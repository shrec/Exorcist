#pragma once

#include "../itool.h"

#include <functional>

// ── GetDiagnosticsTool ────────────────────────────────────────────────────────
// Returns compiler errors and warnings visible in the IDE.

class GetDiagnosticsTool : public ITool
{
public:
    /// diagnosticsGetter is a callable: QList<AgentDiagnostic>(const QString &filePath)
    using DiagnosticsGetter = std::function<QList<AgentDiagnostic>(const QString &filePath)>;

    explicit GetDiagnosticsTool(DiagnosticsGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    DiagnosticsGetter m_getter;
};

// ── GitStatusTool ─────────────────────────────────────────────────────────────
// Returns the git status of the workspace.

class GitStatusTool : public ITool
{
public:
    using GitStatusGetter = std::function<QString()>;

    explicit GitStatusTool(GitStatusGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;

private:
    GitStatusGetter m_getter;
};

// ── GitDiffTool ───────────────────────────────────────────────────────────────
// Returns the git diff for a file or the entire workspace.

class GitDiffTool : public ITool
{
public:
    using GitDiffGetter = std::function<QString(const QString &filePath)>;

    explicit GitDiffTool(GitDiffGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    GitDiffGetter m_getter;
};
