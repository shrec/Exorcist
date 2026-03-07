#pragma once

#include "../itool.h"
#include "../icontextprovider.h"

#include <QJsonArray>

// ── GetDiagnosticsTool ────────────────────────────────────────────────────────
// Returns compiler errors and warnings visible in the IDE.

class GetDiagnosticsTool : public ITool
{
public:
    /// diagnosticsGetter is a callable: QList<AgentDiagnostic>(const QString &filePath)
    using DiagnosticsGetter = std::function<QList<AgentDiagnostic>(const QString &filePath)>;

    explicit GetDiagnosticsTool(DiagnosticsGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("get_diagnostics");
        s.description = QStringLiteral(
            "Get compiler errors, warnings, and diagnostics for a file or "
            "the entire workspace. Use this to understand build errors "
            "before attempting fixes.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional file path. If empty, returns all diagnostics.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const QList<AgentDiagnostic> diags = m_getter(filePath);

        if (diags.isEmpty())
            return {true, {}, QStringLiteral("No diagnostics found."), {}};

        QString result;
        for (const auto &d : diags) {
            QString severity;
            switch (d.severity) {
            case AgentDiagnostic::Severity::Error:   severity = QStringLiteral("ERROR"); break;
            case AgentDiagnostic::Severity::Warning: severity = QStringLiteral("WARN");  break;
            case AgentDiagnostic::Severity::Info:    severity = QStringLiteral("INFO");  break;
            }
            result += QStringLiteral("%1:%2:%3: %4: %5\n")
                          .arg(d.filePath)
                          .arg(d.line)
                          .arg(d.column)
                          .arg(severity, d.message);
        }

        return {true, {},
                QStringLiteral("%1 diagnostic(s):\n%2").arg(diags.size()).arg(result), {}};
    }

private:
    DiagnosticsGetter m_getter;
};

// ── GitStatusTool ─────────────────────────────────────────────────────────────
// Returns the git status of the workspace.

class GitStatusTool : public ITool
{
public:
    using GitStatusGetter = std::function<QString()>;

    explicit GitStatusTool(GitStatusGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("git_status");
        s.description = QStringLiteral(
            "Get the current git status of the workspace, including "
            "modified, staged, and untracked files, and the current branch.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{}},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const QString status = m_getter();
        if (status.isEmpty())
            return {true, {}, QStringLiteral("Not a git repository or no changes."), {}};
        return {true, {}, status, {}};
    }

private:
    GitStatusGetter m_getter;
};

// ── GitDiffTool ───────────────────────────────────────────────────────────────
// Returns the git diff for a file or the entire workspace.

class GitDiffTool : public ITool
{
public:
    using GitDiffGetter = std::function<QString(const QString &filePath)>;

    explicit GitDiffTool(GitDiffGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("git_diff");
        s.description = QStringLiteral(
            "Get the git diff for a specific file or the entire workspace. "
            "Shows what changed compared to HEAD.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional file path. If empty, returns full workspace diff.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const QString diff = m_getter(filePath);
        if (diff.isEmpty())
            return {true, {}, QStringLiteral("No changes detected."), {}};
        return {true, {}, diff, {}};
    }

private:
    GitDiffGetter m_getter;
};
