#pragma once

#include "../itool.h"
#include "luatool.h"

#include <QString>

// ── Agent Tool Store ─────────────────────────────────────────────────────────
// Three tools that let the AI agent create, list, and run its own Lua tools.
// Tools are persisted to an "AgentTools" directory next to the IDE executable.
//
// Each tool is stored as two files:
//   <name>.lua   — the Lua script source
//   <name>.json  — metadata: { "name", "description", "created" }
//
// The agent can build a personal toolkit: save reusable scripts once, then
// invoke them by name in future sessions — the tools survive IDE restarts.

namespace AgentToolStoreDetail {

QString agentToolsDir();
bool ensureDir();
QString sanitizeName(const QString &name);

} // namespace AgentToolStoreDetail


// ── save_lua_tool ────────────────────────────────────────────────────────────
// Saves a Lua script as a named reusable tool.

class SaveLuaToolTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};


// ── list_lua_tools ───────────────────────────────────────────────────────────
// Lists all saved agent tools with their descriptions.

class ListLuaToolsTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;
};


// ── run_lua_tool ─────────────────────────────────────────────────────────────
// Runs a previously saved Lua tool by name.

class RunLuaToolTool : public ITool
{
public:
    using LuaExecutor = LuaExecuteTool::LuaExecutor;

    explicit RunLuaToolTool(LuaExecutor executor);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    LuaExecutor m_executor;
};
