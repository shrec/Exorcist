#pragma once

#include "../itool.h"

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

    DebugSetBreakpointTool(BreakpointSetter setter, BreakpointRemover remover);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

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

    explicit DebugGetStackTraceTool(StackGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

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

    DebugGetVariablesTool(VariablesGetter getter, Evaluator evaluator);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

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

    explicit DebugStepTool(Stepper stepper);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    Stepper m_stepper;
};
