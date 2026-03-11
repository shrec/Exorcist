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

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("run_in_terminal");
        s.description = QStringLiteral(
            "Run a command in a terminal session. Returns stdout/stderr and "
            "exit code. For long-running tasks (servers, watch-mode builds), "
            "set isBackground=true — this returns a session ID immediately. "
            "Use get_terminal_output to check output, await_terminal to wait "
            "for completion, or kill_terminal to stop the process.");
        s.permission  = AgentToolPermission::Dangerous;
        s.timeoutMs   = 60000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("command"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("The command to execute (e.g. 'cmake --build build').")}
                }},
                {QStringLiteral("timeoutMs"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Timeout in milliseconds (default: 30000). "
                                    "Ignored when isBackground=true.")}
                }},
                {QStringLiteral("isBackground"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("If true, run in background and return a session ID "
                                    "immediately. Use get_terminal_output, await_terminal, "
                                    "or kill_terminal to manage. Default: false.")}
                }},
                {QStringLiteral("explanation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("A one-sentence description of what the command does.")}
                }},
                {QStringLiteral("goal"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("A short description of the goal or purpose of the command.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("command")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString command = args[QLatin1String("command")].toString();
        if (command.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing required parameter: command")};

        const bool background = args[QLatin1String("isBackground")].toBool(false);

        // ── Background execution ──────────────────────────────────────────
        if (background && m_sessionMgr) {
            const QString explanation = args[QLatin1String("explanation")].toString();
            const QString sessionId = m_sessionMgr->runBackground(command, explanation);

            QJsonObject data;
            data[QStringLiteral("id")]         = sessionId;
            data[QStringLiteral("isBackground")] = true;

            const QString text = QStringLiteral(
                "Started background session %1.\n"
                "Use get_terminal_output to check output, "
                "await_terminal to wait, or kill_terminal to stop."
            ).arg(sessionId);

            return {true, data, text, {}};
        }

        // ── Foreground execution (blocking) ───────────────────────────────
        if (m_sessionMgr) {
            const int timeout = args[QLatin1String("timeoutMs")].toInt(30000);
            TerminalSession s = m_sessionMgr->runForeground(command, timeout);

            QJsonObject data;
            data[QStringLiteral("exitCode")] = s.exitCode;
            data[QStringLiteral("timedOut")] = s.timedOut;

            QString text;
            if (s.timedOut)
                text = QStringLiteral("Command timed out after %1ms.\n").arg(timeout);
            else
                text = QStringLiteral("Exit code: %1\n").arg(s.exitCode);

            if (!s.stdOut.isEmpty())
                text += QStringLiteral("stdout:\n%1\n").arg(s.stdOut);
            if (!s.stdErr.isEmpty())
                text += QStringLiteral("stderr:\n%1\n").arg(s.stdErr);

            return {!s.timedOut && s.exitCode == 0, data, text,
                    s.exitCode == 0 ? QString() : s.stdErr};
        }

        // ── Fallback: use IProcess directly ───────────────────────────────
        const int timeout = args[QLatin1String("timeoutMs")].toInt(30000);
        QStringList cmdArgs;
#ifdef Q_OS_WIN
        const QString program = QStringLiteral("cmd");
        cmdArgs << QStringLiteral("/c") << command;
#else
        const QString program = QStringLiteral("sh");
        cmdArgs << QStringLiteral("-c") << command;
#endif

        ProcessResult pr = m_process->run(program, cmdArgs, timeout);

        QJsonObject data;
        data[QStringLiteral("exitCode")] = pr.exitCode;
        data[QStringLiteral("timedOut")] = pr.timedOut;

        QString text;
        if (pr.timedOut)
            text = QStringLiteral("Command timed out after %1ms.\n").arg(timeout);
        else
            text = QStringLiteral("Exit code: %1\n").arg(pr.exitCode);

        if (!pr.stdOut.isEmpty())
            text += QStringLiteral("stdout:\n%1\n").arg(pr.stdOut);
        if (!pr.stdErr.isEmpty())
            text += QStringLiteral("stderr:\n%1\n").arg(pr.stdErr);

        return {pr.success, data, text, pr.success ? QString() : pr.stdErr};
    }

private:
    IProcess *m_process;
    QString   m_workDir;
    TerminalSessionManager *m_sessionMgr = nullptr;
};
