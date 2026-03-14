#pragma once

#include "../itool.h"
#include "../terminalsessionmanager.h"
#include "../../core/iprocess.h"

#include <QJsonArray>

// ── RunCommandTool ────────────────────────────────────────────────────────────
// Runs a terminal command and returns the output.
// Supports both foreground (blocking) and background (async) execution.
// Background sessions return a session ID for use with get_terminal_output,
// await_terminal, and kill_terminal.

class RunCommandTool : public ITool
{
public:
    explicit RunCommandTool(IProcess *process, const QString &workDir = {},
                            TerminalSessionManager *sessionMgr = nullptr)
        : m_process(process), m_workDir(workDir), m_sessionMgr(sessionMgr) {}

    void setWorkingDirectory(const QString &dir) { m_workDir = dir; }
    void setSessionManager(TerminalSessionManager *mgr) { m_sessionMgr = mgr; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
    void cancel() override;

private:
    IProcess *m_process;
    QString   m_workDir;
    TerminalSessionManager *m_sessionMgr = nullptr;
};
