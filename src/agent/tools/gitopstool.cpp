#include "gitopstool.h"

GitOpsTool::GitOpsTool(GitExecutor executor)
    : m_executor(std::move(executor))
{}

ToolSpec GitOpsTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("git_ops");
    s.description = QStringLiteral(
        "Perform Git operations on the repository. Use this to save work, "
        "create branches, investigate history, and manage changes.\n\n"
        "Operations:\n"
        "  add — Stage files. Args: {files: [\"path1\", \"path2\"] or [\".\"]}\n"
        "  commit — Commit staged changes. Args: {message: \"...\"}\n"
        "  branch — Create/list/delete branches. Args: {name?, delete?, list?}\n"
        "  checkout — Switch branch or restore files. Args: {target: \"branch-or-file\"}\n"
        "  merge — Merge a branch into current. Args: {branch, message?}\n"
        "  rebase — Rebase current branch onto another. Args: {branch}\n"
        "  pull — Pull from remote. Args: {remote?: \"origin\", branch?}\n"
        "  push — Push current branch to remote (no force). Args: {remote?: \"origin\", branch?, setUpstream?}\n"
        "  remote — List/show remotes. Args: {action: \"list\"|\"show\", name?}\n"
        "  status — Show working tree status. Args: {}\n"
        "  diff — Show diff. Args: {staged?, filePath?, commitA?, commitB?}\n"
        "  stash — Stash/pop working changes. Args: {action: \"push\"|\"pop\"|\"list\"|\"drop\"}\n"
        "  blame — Show line-by-line authorship. Args: {filePath, startLine?, endLine?}\n"
        "  log — Browse commit history. Args: {count?: 10, filePath?, author?, "
        "since?, grep?}\n"
        "  tag — Create/list tags. Args: {name?, message?, list?}\n"
        "  cherry_pick — Cherry-pick a commit. Args: {commitHash}\n"
        "  reset — Unstage files (soft). Args: {files: [\"path\"]}\n\n"
        "Force push and hard reset are blocked.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 60000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("add"),
                    QStringLiteral("commit"),
                    QStringLiteral("branch"),
                    QStringLiteral("checkout"),
                    QStringLiteral("merge"),
                    QStringLiteral("rebase"),
                    QStringLiteral("pull"),
                    QStringLiteral("push"),
                    QStringLiteral("remote"),
                    QStringLiteral("status"),
                    QStringLiteral("diff"),
                    QStringLiteral("stash"),
                    QStringLiteral("blame"),
                    QStringLiteral("log"),
                    QStringLiteral("tag"),
                    QStringLiteral("cherry_pick"),
                    QStringLiteral("reset")
                }},
                {QStringLiteral("description"),
                 QStringLiteral("The Git operation to perform.")}
            }},
            {QStringLiteral("message"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Commit/tag/merge message.")}
            }},
            {QStringLiteral("branch"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Branch name for merge/rebase/pull/push.")}
            }},
            {QStringLiteral("remote"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Remote name (default: 'origin') for pull/push/remote.")}
            }},
            {QStringLiteral("setUpstream"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Set upstream tracking ref when pushing (git push -u).")}
            }},
            {QStringLiteral("staged"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Show staged diff (git diff --cached).")}
            }},
            {QStringLiteral("commitA"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("First commit ref for diff range.")}
            }},
            {QStringLiteral("commitB"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Second commit ref for diff range.")}
            }},
            {QStringLiteral("action"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Sub-action for stash (push/pop/list/drop) or remote (list/show).")}
            }},
            {QStringLiteral("files"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("File paths for add/reset.")}
            }},
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File path for blame/log/diff.")}
            }},
            {QStringLiteral("name"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Branch/tag name.")}
            }},
            {QStringLiteral("target"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Checkout target (branch name or file path).")}
            }},
            {QStringLiteral("commitHash"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Commit hash for cherry_pick.")}
            }},
            {QStringLiteral("count"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Number of log entries (default: 10, max: 50).")}
            }},
            {QStringLiteral("startLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Start line for blame range.")}
            }},
            {QStringLiteral("endLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("End line for blame range.")}
            }},
            {QStringLiteral("author"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Filter log by author.")}
            }},
            {QStringLiteral("since"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Filter log since date (e.g. \"2024-01-01\").")}
            }},
            {QStringLiteral("grep"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Filter log by message pattern.")}
            }},
            {QStringLiteral("list"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("List branches/tags instead of create.")}
            }},
            {QStringLiteral("delete"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Delete a branch (safe delete only, not force).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult GitOpsTool::invoke(const QJsonObject &args)
{
    const QString operation = args[QLatin1String("operation")].toString();
    if (operation.isEmpty())
        return {false, {}, {}, QStringLiteral("'operation' is required.")};

    const GitResult result = m_executor(operation, args);

    if (!result.ok)
        return {false, {}, {},
                QStringLiteral("git %1 failed: %2").arg(operation, result.error)};

    QString text = result.output;
    if (text.isEmpty())
        text = QStringLiteral("git %1: OK").arg(operation);

    // Truncate large output (blame, log, diff)
    if (text.size() > 30000) {
        text.truncate(30000);
        text += QStringLiteral("\n... (truncated)");
    }

    return {true, {}, text, {}};
}
