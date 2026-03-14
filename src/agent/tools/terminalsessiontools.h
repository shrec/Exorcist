#pragma once

#include "../itool.h"

#include <functional>

class TerminalSessionManager;

// ── GetTerminalOutputTool ─────────────────────────────────────────────────────
// Retrieves stdout/stderr from a background terminal session by ID.

class GetTerminalOutputTool : public ITool
{
public:
    explicit GetTerminalOutputTool(TerminalSessionManager *mgr);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};

// ── KillTerminalTool ──────────────────────────────────────────────────────────
// Kills a running background terminal session.

class KillTerminalTool : public ITool
{
public:
    explicit KillTerminalTool(TerminalSessionManager *mgr);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};

// ── AwaitTerminalTool ─────────────────────────────────────────────────────────
// Waits for a background terminal session to finish, with optional timeout.

class AwaitTerminalTool : public ITool
{
public:
    explicit AwaitTerminalTool(TerminalSessionManager *mgr);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};

// ── TerminalSelectionTool ─────────────────────────────────────────────────────
// Returns text currently selected in the active terminal tab.

class TerminalSelectionTool : public ITool
{
public:
    using SelectionGetter = std::function<QString()>;

    explicit TerminalSelectionTool(SelectionGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SelectionGetter m_getter;
};

// ── TerminalLastCommandTool ───────────────────────────────────────────────────
// Returns the output of the most recently run command in the foreground terminal.

class TerminalLastCommandTool : public ITool
{
public:
    using OutputGetter = std::function<QString()>;

    explicit TerminalLastCommandTool(OutputGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    OutputGetter m_getter;
};
