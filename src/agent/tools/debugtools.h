#pragma once

#include "../itool.h"

#include <QJsonArray>
#include <functional>

// ── DebugSetBreakpointTool ───────────────────────────────────────────────────
// Set or remove a breakpoint at file:line with optional condition.

class DebugSetBreakpointTool : public ITool
{
public:
    // setter(filePath, line, condition) → breakpoint id (empty on failure)
    // remover(filePath, line) → success
    using BreakpointSetter  = std::function<QString(const QString &, int, const QString &)>;
    using BreakpointRemover = std::function<bool(const QString &, int)>;

    DebugSetBreakpointTool(BreakpointSetter setter, BreakpointRemover remover)
        : m_setter(std::move(setter)), m_remover(std::move(remover)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("debug_set_breakpoint");
        s.description = QStringLiteral(
            "Set or remove a debugger breakpoint at a specific file and line. "
            "Requires an active debug session. Use 'remove' flag to delete "
            "an existing breakpoint.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Absolute path to the source file.")}
                }},
                {QStringLiteral("line"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based line number for the breakpoint.")}
                }},
                {QStringLiteral("condition"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional conditional expression (e.g. \"i > 10\").")}
                }},
                {QStringLiteral("remove"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("If true, remove the breakpoint at this location.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("filePath"), QStringLiteral("line")
            }}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const int line         = args[QLatin1String("line")].toInt();
        const bool remove      = args[QLatin1String("remove")].toBool(false);

        if (filePath.isEmpty() || line < 1)
            return {false, {}, {}, QStringLiteral("filePath and line are required.")};

        if (remove) {
            const bool ok = m_remover(filePath, line);
            if (ok)
                return {true, {}, QStringLiteral("Breakpoint removed at %1:%2").arg(filePath).arg(line), {}};
            return {false, {}, {}, QStringLiteral("No breakpoint found at %1:%2").arg(filePath).arg(line)};
        }

        const QString cond = args[QLatin1String("condition")].toString();
        const QString id   = m_setter(filePath, line, cond);
        if (id.isEmpty())
            return {false, {}, {},
                    QStringLiteral("Failed to set breakpoint. Is a debug session active?")};

        QString msg = QStringLiteral("Breakpoint set at %1:%2").arg(filePath).arg(line);
        if (!cond.isEmpty())
            msg += QStringLiteral(" (condition: %1)").arg(cond);
        return {true, {}, msg, {}};
    }

private:
    BreakpointSetter  m_setter;
    BreakpointRemover m_remover;
};

// ── DebugGetStackTraceTool ───────────────────────────────────────────────────
// Get the current call stack from the debugger.

class DebugGetStackTraceTool : public ITool
{
public:
    // stackGetter(threadId) → formatted stack trace text
    using StackGetter = std::function<QString(int)>;

    explicit DebugGetStackTraceTool(StackGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("debug_get_stack_trace");
        s.description = QStringLiteral(
            "Get the current call stack from the debugger. Shows function "
            "names, source files, and line numbers for each frame. "
            "Requires an active, paused debug session.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("threadId"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Thread ID (default: current stopped thread).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const int threadId = args[QLatin1String("threadId")].toInt(-1);
        const QString stack = m_getter(threadId);
        if (stack.isEmpty())
            return {false, {}, {},
                    QStringLiteral("No stack trace available. "
                                   "Is the debugger paused at a breakpoint?")};
        return {true, {}, stack, {}};
    }

private:
    StackGetter m_getter;
};

// ── DebugGetVariablesTool ────────────────────────────────────────────────────
// Get local variables or evaluate an expression in the debugger.

class DebugGetVariablesTool : public ITool
{
public:
    // varsGetter(frameId) → formatted locals text
    using VariablesGetter = std::function<QString(int)>;
    // evaluator(expression, frameId) → result text
    using Evaluator = std::function<QString(const QString &, int)>;

    DebugGetVariablesTool(VariablesGetter getter, Evaluator evaluator)
        : m_getter(std::move(getter)), m_evaluator(std::move(evaluator)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("debug_get_variables");
        s.description = QStringLiteral(
            "Get local variables at the current stack frame, or evaluate "
            "a specific expression. Use 'expression' to inspect a variable, "
            "call a function, or compute a value in the debugger context.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("frameId"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Stack frame ID (default: topmost frame).")}
                }},
                {QStringLiteral("expression"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Expression to evaluate (e.g. \"myVar.size()\"). "
                                    "If omitted, returns all local variables.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const int frameId       = args[QLatin1String("frameId")].toInt(0);
        const QString expression = args[QLatin1String("expression")].toString();

        if (!expression.isEmpty()) {
            const QString result = m_evaluator(expression, frameId);
            if (result.isEmpty())
                return {false, {}, {},
                        QStringLiteral("Evaluation failed. Is the debugger paused?")};
            return {true, {}, QStringLiteral("%1 = %2").arg(expression, result), {}};
        }

        const QString vars = m_getter(frameId);
        if (vars.isEmpty())
            return {false, {}, {},
                    QStringLiteral("No variables available. "
                                   "Is the debugger paused at a breakpoint?")};
        return {true, {}, vars, {}};
    }

private:
    VariablesGetter m_getter;
    Evaluator       m_evaluator;
};

// ── DebugStepTool ────────────────────────────────────────────────────────────
// Control debugger execution: continue, step over, step into, step out, pause.

class DebugStepTool : public ITool
{
public:
    // stepper(action) → success
    using Stepper = std::function<bool(const QString &action)>;

    explicit DebugStepTool(Stepper stepper)
        : m_stepper(std::move(stepper)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("debug_step");
        s.description = QStringLiteral(
            "Control debugger execution. Actions: "
            "'continue' (resume), 'over' (step over line), "
            "'into' (step into function), 'out' (step out of function), "
            "'pause' (pause running program).");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("action"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: continue, over, into, out, pause")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("continue"), QStringLiteral("over"),
                        QStringLiteral("into"), QStringLiteral("out"),
                        QStringLiteral("pause")
                    }}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString action = args[QLatin1String("action")].toString();
        if (action.isEmpty())
            return {false, {}, {}, QStringLiteral("'action' parameter is required.")};

        const bool ok = m_stepper(action);
        if (!ok)
            return {false, {}, {},
                    QStringLiteral("Debug action '%1' failed. Is a debug session active?").arg(action)};
        return {true, {}, QStringLiteral("Executed: %1").arg(action), {}};
    }

private:
    Stepper m_stepper;
};
