#include "terminalsessiontools.h"
#include "../terminalsessionmanager.h"

#include <QJsonArray>
#include <QJsonObject>

// ── GetTerminalOutputTool ────────────────────────────────────────────────────

GetTerminalOutputTool::GetTerminalOutputTool(TerminalSessionManager *mgr)
    : m_mgr(mgr)
{
}

ToolSpec GetTerminalOutputTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("get_terminal_output");
    s.description = QStringLiteral(
        "Get the output of a terminal command previously started in the "
        "background with run_in_terminal (isBackground=true). Returns "
        "stdout, stderr, running status, and exit code.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("id"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The session ID returned by run_in_terminal when isBackground=true.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("id")}}
    };
    return s;
}

ToolExecResult GetTerminalOutputTool::invoke(const QJsonObject &args)
{
    const QString id = args[QLatin1String("id")].toString();
    if (id.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: id")};

    if (!m_mgr->hasSession(id))
        return {false, {}, {}, QStringLiteral("No terminal session with id: %1").arg(id)};

    const TerminalSession s = m_mgr->session(id);

    QJsonObject data;
    data[QStringLiteral("id")]       = s.id;
    data[QStringLiteral("running")]  = s.running;
    data[QStringLiteral("exitCode")] = s.exitCode;
    data[QStringLiteral("command")]  = s.command;

    QString text;
    if (s.running)
        text = QStringLiteral("Session %1 is still running.\n").arg(id);
    else
        text = QStringLiteral("Session %1 finished with exit code %2.\n")
                   .arg(id).arg(s.exitCode);

    if (!s.stdOut.isEmpty())
        text += QStringLiteral("stdout:\n%1\n").arg(s.stdOut);
    if (!s.stdErr.isEmpty())
        text += QStringLiteral("stderr:\n%1\n").arg(s.stdErr);

    return {true, data, text, {}};
}

// ── KillTerminalTool ─────────────────────────────────────────────────────────

KillTerminalTool::KillTerminalTool(TerminalSessionManager *mgr)
    : m_mgr(mgr)
{
}

ToolSpec KillTerminalTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("kill_terminal");
    s.description = QStringLiteral(
        "Kill a running background terminal session by ID. "
        "Use when a long-running process needs to be stopped.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("id"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The session ID of the terminal to kill.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("id")}}
    };
    return s;
}

ToolExecResult KillTerminalTool::invoke(const QJsonObject &args)
{
    const QString id = args[QLatin1String("id")].toString();
    if (id.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: id")};

    if (!m_mgr->hasSession(id))
        return {false, {}, {}, QStringLiteral("No terminal session with id: %1").arg(id)};

    const bool killed = m_mgr->killSession(id);
    QJsonObject data;
    data[QStringLiteral("id")]     = id;
    data[QStringLiteral("killed")] = killed;

    const QString text = killed
        ? QStringLiteral("Killed terminal session %1.").arg(id)
        : QStringLiteral("Session %1 was not running.").arg(id);

    return {true, data, text, {}};
}

// ── AwaitTerminalTool ────────────────────────────────────────────────────────

AwaitTerminalTool::AwaitTerminalTool(TerminalSessionManager *mgr)
    : m_mgr(mgr)
{
}

ToolSpec AwaitTerminalTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("await_terminal");
    s.description = QStringLiteral(
        "Wait for a background terminal command to finish. Returns the "
        "final output once the process completes or the timeout elapses. "
        "If timeout is reached, the session stays running.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 120000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("id"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The session ID of the background terminal to wait for.")}
            }},
            {QStringLiteral("timeoutMs"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum time to wait in milliseconds (default: 60000).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("id")}}
    };
    return s;
}

ToolExecResult AwaitTerminalTool::invoke(const QJsonObject &args)
{
    const QString id = args[QLatin1String("id")].toString();
    if (id.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: id")};

    if (!m_mgr->hasSession(id))
        return {false, {}, {}, QStringLiteral("No terminal session with id: %1").arg(id)};

    const int timeout = args[QLatin1String("timeoutMs")].toInt(60000);
    const TerminalSession s = m_mgr->awaitSession(id, timeout);

    QJsonObject data;
    data[QStringLiteral("id")]       = s.id;
    data[QStringLiteral("running")]  = s.running;
    data[QStringLiteral("exitCode")] = s.exitCode;
    data[QStringLiteral("command")]  = s.command;

    QString text;
    if (s.running)
        text = QStringLiteral("Timeout reached. Session %1 is still running.\n").arg(id);
    else
        text = QStringLiteral("Session %1 finished with exit code %2.\n")
                   .arg(id).arg(s.exitCode);

    if (!s.stdOut.isEmpty())
        text += QStringLiteral("stdout:\n%1\n").arg(s.stdOut);
    if (!s.stdErr.isEmpty())
        text += QStringLiteral("stderr:\n%1\n").arg(s.stdErr);

    return {!s.running, data, text, {}};
}

// ── TerminalSelectionTool ────────────────────────────────────────────────────

TerminalSelectionTool::TerminalSelectionTool(SelectionGetter getter)
    : m_getter(std::move(getter))
{
}

ToolSpec TerminalSelectionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("terminal_selection");
    s.description = QStringLiteral(
        "Get the text currently selected (highlighted) in the active "
        "terminal. Returns empty if nothing is selected.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 3000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult TerminalSelectionTool::invoke(const QJsonObject &)
{
    const QString text = m_getter();
    if (text.isEmpty())
        return {true, {}, QStringLiteral("No text is currently selected in the terminal."), {}};
    return {true, {}, text, {}};
}

// ── TerminalLastCommandTool ──────────────────────────────────────────────────

TerminalLastCommandTool::TerminalLastCommandTool(OutputGetter getter)
    : m_getter(std::move(getter))
{
}

ToolSpec TerminalLastCommandTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("terminal_last_command");
    s.description = QStringLiteral(
        "Get the output of the last command run in the active terminal. "
        "Returns the most recent lines of terminal output.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult TerminalLastCommandTool::invoke(const QJsonObject &)
{
    const QString output = m_getter();
    if (output.isEmpty())
        return {true, {}, QStringLiteral("No recent terminal output."), {}};
    return {true, {}, output, {}};
}
