#pragma once

#include "../itool.h"

#include <functional>
#include <memory>

class AgentController;
class AgentOrchestrator;
class IContextProvider;
class ToolRegistry;

// ── SubagentTool ──────────────────────────────────────────────────────────────
//
// Spawns a child AgentController that autonomously executes a complex task.
// The child runs with the same tool registry, orchestrator, and context
// provider as the parent, but owns its own session and conversation history.
//
// Depth is limited to prevent unbounded recursion: max 3 levels deep.
//
// Usage: the parent agent calls "run_subagent" with a detailed prompt.
// The child agent runs an independent turn, collecting tool calls and
// responses, then returns its final answer as the tool result.

class SubagentTool : public ITool
{
public:
    /// Factory: creates a new AgentController for the sub-turn.
    /// (filePath, line, column, newName) → JSON string of workspace edits
    using ControllerFactory = std::function<
        std::unique_ptr<AgentController>(int depth)>;

    explicit SubagentTool(AgentOrchestrator *orchestrator,
                          ToolRegistry *toolRegistry,
                          IContextProvider *contextProvider,
                          int currentDepth = 0);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentOrchestrator  *m_orchestrator;
    ToolRegistry       *m_toolRegistry;
    IContextProvider   *m_contextProvider;
    int                 m_currentDepth;

    static constexpr int kMaxDepth = 3;
};
