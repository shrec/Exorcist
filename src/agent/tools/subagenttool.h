#pragma once

#include "../itool.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

#include <functional>
#include <memory>

class AgentController;
class AgentOrchestrator;
class IContextProvider;
class ToolRegistry;

Q_DECLARE_LOGGING_CATEGORY(lcSubagent)

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
                          int currentDepth = 0)
        : m_orchestrator(orchestrator)
        , m_toolRegistry(toolRegistry)
        , m_contextProvider(contextProvider)
        , m_currentDepth(currentDepth)
    {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("run_subagent");
        s.description = QStringLiteral(
            "Launch a sub-agent to handle a complex, multi-step task "
            "autonomously. The sub-agent runs with full access to tools "
            "and returns its final result. Use for tasks that require "
            "multiple tool calls or extensive research. The sub-agent "
            "does not share conversation history with the parent — "
            "provide a detailed, self-contained prompt.\n\n"
            "Good for:\n"
            "- Searching the codebase for specific patterns\n"
            "- Multi-file refactoring tasks\n"
            "- Research that requires reading many files\n"
            "- Tasks that can run independently");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 120000; // 2 minutes — subagent may run many steps
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("prompt"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral(
                         "A detailed description of the task for the "
                         "sub-agent to perform. Must be self-contained — "
                         "include all context the sub-agent needs.")}
                }},
                {QStringLiteral("description"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("A short (3-5 word) description of the task.")}
                }},
                {QStringLiteral("maxSteps"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral(
                         "Maximum number of steps the sub-agent can take. "
                         "Default: 15. Max: 25.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("prompt")
            }}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentOrchestrator  *m_orchestrator;
    ToolRegistry       *m_toolRegistry;
    IContextProvider   *m_contextProvider;
    int                 m_currentDepth;

    static constexpr int kMaxDepth = 3;
};

#ifdef SUBAGENTTOOL_IMPL

Q_LOGGING_CATEGORY(lcSubagent, "exorcist.agent.subagent")

#include "../agentcontroller.h"

ToolExecResult SubagentTool::invoke(const QJsonObject &args)
{
    const QString prompt      = args[QLatin1String("prompt")].toString();
    const QString description = args[QLatin1String("description")].toString(
        QStringLiteral("subagent task"));
    int maxSteps = args[QLatin1String("maxSteps")].toInt(15);
    maxSteps = qBound(1, maxSteps, 25);

    if (prompt.isEmpty())
        return {false, {}, {},
                QStringLiteral("'prompt' is required — provide a detailed task description")};

    // Depth check
    if (m_currentDepth >= kMaxDepth)
        return {false, {}, {},
                QStringLiteral("Maximum sub-agent depth (%1) reached. "
                               "Cannot spawn further sub-agents.")
                    .arg(kMaxDepth)};

    qCInfo(lcSubagent) << "Spawning sub-agent at depth" << (m_currentDepth + 1)
                       << ":" << description;

    // Create a child controller with its own session
    auto childController = std::make_unique<AgentController>(
        m_orchestrator, m_toolRegistry, m_contextProvider);

    childController->setMaxStepsPerTurn(maxSteps);
    childController->setMaxToolPermission(AgentToolPermission::SafeMutate);

    // Set a system prompt for the subagent
    childController->setSystemPrompt(QStringLiteral(
        "You are a sub-agent performing a specific task. "
        "Complete the task thoroughly and report your findings in "
        "your final message. Be concise but thorough. "
        "You have access to the same workspace tools as the parent agent.\n\n"
        "Task description: %1").arg(description));

    childController->newSession(true /* agentMode */);

    // Collect the final result
    QString resultText;
    QString errorText;
    bool finished = false;

    QEventLoop loop;

    QObject::connect(childController.get(), &AgentController::turnFinished,
                     &loop, [&](const AgentTurn &turn) {
        // Extract the final answer from the last step
        for (auto it = turn.steps.rbegin(); it != turn.steps.rend(); ++it) {
            if (it->type == AgentStep::Type::FinalAnswer) {
                resultText = it->finalText;
                break;
            }
        }
        if (resultText.isEmpty()) {
            // Fallback: use the last model response
            for (auto it = turn.steps.rbegin(); it != turn.steps.rend(); ++it) {
                if (it->type == AgentStep::Type::ModelCall &&
                    !it->modelResponse.isEmpty()) {
                    resultText = it->modelResponse;
                    break;
                }
            }
        }
        finished = true;
        loop.quit();
    });

    QObject::connect(childController.get(), &AgentController::turnError,
                     &loop, [&](const QString &error) {
        errorText = error;
        finished = true;
        loop.quit();
    });

    // Timeout safety (2 minutes)
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        childController->cancel();
        errorText = QStringLiteral("Sub-agent timed out after 120 seconds.");
        finished = true;
        loop.quit();
    });
    timeout.start(120000);

    // Kick off the sub-agent turn
    childController->sendMessage(prompt);

    if (!finished)
        loop.exec();

    qCInfo(lcSubagent) << "Sub-agent finished. Success:" << errorText.isEmpty()
                       << "Result length:" << resultText.size();

    if (!errorText.isEmpty())
        return {false, {}, {}, errorText};

    if (resultText.isEmpty())
        return {false, {}, {},
                QStringLiteral("Sub-agent completed but produced no output.")};

    QJsonObject data;
    data[QStringLiteral("result")] = resultText;

    return {true, data, resultText, {}};
}

#endif // SUBAGENTTOOL_IMPL
