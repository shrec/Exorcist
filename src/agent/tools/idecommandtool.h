#pragma once

#include "../itool.h"

#include <functional>

// ── RunIdeCommandTool ─────────────────────────────────────────────────────────
// Executes an IDE command by its string ID (analogous to VS Code's
// `run_vscode_command`).  Commands are registered in ICommandService by
// core subsystems and plugins.  The agent can list available commands and
// then invoke one.

class RunIdeCommandTool : public ITool
{
public:
    using CommandExecutor = std::function<bool(const QString &commandId)>;
    using CommandLister   = std::function<QStringList()>;

    explicit RunIdeCommandTool(CommandExecutor executor,
                               CommandLister   lister);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    CommandExecutor m_executor;
    CommandLister   m_lister;
};
