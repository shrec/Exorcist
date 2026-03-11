#pragma once

#include "../itool.h"
#include "../terminalsessionmanager.h"

#include <QJsonArray>

// ── GetTerminalOutputTool ─────────────────────────────────────────────────────
// Retrieves stdout/stderr from a background terminal session by ID.

class GetTerminalOutputTool : public ITool
{
public:
    explicit GetTerminalOutputTool(TerminalSessionManager *mgr)
        : m_mgr(mgr) {}

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &args) override
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

private:
    TerminalSessionManager *m_mgr;
};

// ── KillTerminalTool ──────────────────────────────────────────────────────────
// Kills a running background terminal session.

class KillTerminalTool : public ITool
{
public:
    explicit KillTerminalTool(TerminalSessionManager *mgr)
        : m_mgr(mgr) {}

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &args) override
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

private:
    TerminalSessionManager *m_mgr;
};

// ── AwaitTerminalTool ─────────────────────────────────────────────────────────
// Waits for a background terminal session to finish, with optional timeout.

class AwaitTerminalTool : public ITool
{
public:
    explicit AwaitTerminalTool(TerminalSessionManager *mgr)
        : m_mgr(mgr) {}

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &args) override
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

private:
    TerminalSessionManager *m_mgr;
};

// ── TerminalSelectionTool ─────────────────────────────────────────────────────
// Returns text currently selected in the active terminal tab.

class TerminalSelectionTool : public ITool
{
public:
    using SelectionGetter = std::function<QString()>;

    explicit TerminalSelectionTool(SelectionGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &) override
    {
        const QString text = m_getter();
        if (text.isEmpty())
            return {true, {}, QStringLiteral("No text is currently selected in the terminal."), {}};
        return {true, {}, text, {}};
    }

private:
    SelectionGetter m_getter;
};

// ── TerminalLastCommandTool ───────────────────────────────────────────────────
// Returns the output of the most recently run command in the foreground terminal.

class TerminalLastCommandTool : public ITool
{
public:
    using OutputGetter = std::function<QString()>;

    explicit TerminalLastCommandTool(OutputGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &) override
    {
        const QString output = m_getter();
        if (output.isEmpty())
            return {true, {}, QStringLiteral("No recent terminal output."), {}};
        return {true, {}, output, {}};
    }

private:
    OutputGetter m_getter;
};
