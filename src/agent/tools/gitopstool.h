#pragma once

#include "../itool.h"

#include <functional>

// ── GitOpsTool ──────────────────────────────────────────────────────────────
// Full Git operations beyond read-only status/diff. Lets the agent commit,
// branch, stash, blame, and browse history — essential for autonomous
// development workflows (checkpoint work, create feature branches, etc.).

class GitOpsTool : public ITool
{
public:
    struct GitResult {
        bool    ok;
        QString output;
        QString error;
    };

    // executor(operation, args) → GitResult
    //   operation: "commit", "branch", "stash", "blame", "log", "checkout",
    //              "add", "reset", "tag", "cherry_pick"
    //   args: operation-specific arguments as JSON
    using GitExecutor = std::function<GitResult(
        const QString &operation, const QJsonObject &args)>;

    explicit GitOpsTool(GitExecutor executor);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    GitExecutor m_executor;
};
