#pragma once

#include "../itool.h"
#include "../../core/iprocess.h"

#include <QJsonArray>

// ── RunCommandTool ────────────────────────────────────────────────────────────
// Runs a terminal command and returns the output.
// This is a DANGEROUS tool — requires user confirmation.

class RunCommandTool : public ITool
{
public:
    explicit RunCommandTool(IProcess *process, const QString &workDir = {})
        : m_process(process), m_workDir(workDir) {}

    void setWorkingDirectory(const QString &dir) { m_workDir = dir; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("run_in_terminal");
        s.description = QStringLiteral(
            "Run a natural language command in a persistent terminal session. "
            "Use this to execute build commands, tests, scripts, or any shell "
            "operation. Returns stdout/stderr and exit code. "
            "For long-running tasks, set isBackground=true.");
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
                     QStringLiteral("Timeout in milliseconds (default: 30000).")}
                }},
                {QStringLiteral("isBackground"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Whether to run the command in the background (non-blocking). Default: false.")}
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

        const int timeout = args[QLatin1String("timeoutMs")].toInt(30000);

        // Split command into program + args
        // On Windows, use cmd /c; on Unix, use sh -c
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
        if (pr.timedOut) {
            text = QStringLiteral("Command timed out after %1ms.\n").arg(timeout);
        } else {
            text = QStringLiteral("Exit code: %1\n").arg(pr.exitCode);
        }

        if (!pr.stdOut.isEmpty())
            text += QStringLiteral("stdout:\n%1\n").arg(pr.stdOut);
        if (!pr.stdErr.isEmpty())
            text += QStringLiteral("stderr:\n%1\n").arg(pr.stdErr);

        return {pr.success, data, text, pr.success ? QString() : pr.stdErr};
    }

private:
    IProcess *m_process;
    QString   m_workDir;
};
