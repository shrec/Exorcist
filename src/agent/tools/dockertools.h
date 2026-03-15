#pragma once

#include "../itool.h"
#include "../../core/iprocess.h"

// ── DockerTool ────────────────────────────────────────────────────────────────
// Multi-action Docker management tool. Supports: ps, build, run, exec, stop,
// rm, images, logs, inspect. All operations go through IProcess::run("docker").

class DockerTool : public ITool
{
public:
    explicit DockerTool(IProcess *process);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult runDocker(const QStringList &args, int timeoutMs);

    IProcess *m_process;
    QString   m_workspaceRoot;
};
