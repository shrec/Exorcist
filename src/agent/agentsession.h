#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>

#include "../aiinterface.h"

// ── Patch proposal ────────────────────────────────────────────────────────────
//
// Represents a structured code change (edit, create, delete) proposed by
// the agent. Changes are NOT applied directly — they go through a
// preview/accept/reject flow controlled by the UI.

struct PatchHunk
{
    int     startLine = -1;
    int     endLine   = -1;         // -1 means "insert after startLine"
    QString oldText;                // original text (for display/verify)
    QString newText;                // replacement text
};

struct PatchProposal
{
    enum class Action { Edit, Create, Delete, Rename };

    Action         action = Action::Edit;
    QString        filePath;
    QString        newFilePath;       // only for Rename
    QList<PatchHunk> hunks;           // only for Edit (may have multiple hunks)
    QString        fullContent;       // for Create: the full file content
    QString        description;       // human-readable summary of change

    enum class Status { Pending, Accepted, Rejected, Applied, Failed };
    Status         status = Status::Pending;
};

// ── Agent step ────────────────────────────────────────────────────────────────
//
// One step in the agent loop. Each step is either:
//   - A model call + response
//   - A tool invocation + result
//   - A final answer

struct AgentStep
{
    enum class Type
    {
        ModelCall,      // sent request to LLM, received response
        ToolCall,       // model requested a tool, we executed it
        FinalAnswer,    // agent produced its final response
        Error,          // something went wrong
    };

    Type    type;
    QString timestamp;

    // For ModelCall
    QString            modelRequest;   // what we sent (summary)
    QString            modelResponse;  // what the model said

    // For ToolCall
    ToolCall           toolCall;       // the tool call from model
    QString            toolResult;     // what the tool returned
    bool               toolSuccess = true;

    // For FinalAnswer
    QString            finalText;
    QList<PatchProposal> patches;

    // For Error
    QString            errorMessage;
};

// ── Turn ──────────────────────────────────────────────────────────────────────
// One user turn = user message + all agent steps to produce a response.

struct AgentTurn
{
    QString             userMessage;
    QList<AgentStep>    steps;
    QList<PatchProposal> patchProposals;  // accumulated across steps

    bool isComplete() const
    {
        if (steps.isEmpty()) return false;
        const auto lastType = steps.last().type;
        return lastType == AgentStep::Type::FinalAnswer
            || lastType == AgentStep::Type::Error;
    }
};

// ── Agent session ─────────────────────────────────────────────────────────────
//
// Persistent session that tracks the full conversation, including all
// turns, tool calls, file changes, and intermediate state.
//
// Each session has:
//   - A unique ID
//   - A list of turns (user message + agent steps)
//   - A running conversation history (for model context)
//   - Tracked file changes (for undo/rollback)
//   - Agent mode flag

class AgentSession
{
public:
    AgentSession()
        : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_createdAt(QDateTime::currentDateTime())
    {}

    // ── Identity ──────────────────────────────────────────────────────────
    QString id() const { return m_id; }
    QDateTime createdAt() const { return m_createdAt; }

    // ── Mode ──────────────────────────────────────────────────────────────
    bool agentMode() const { return m_agentMode; }
    void setAgentMode(bool on) { m_agentMode = on; }

    // ── Provider ──────────────────────────────────────────────────────────
    QString providerId() const { return m_providerId; }
    void setProviderId(const QString &id) { m_providerId = id; }

    QString model() const { return m_model; }
    void setModel(const QString &model) { m_model = model; }

    // ── Turns ─────────────────────────────────────────────────────────────
    const QList<AgentTurn> &turns() const { return m_turns; }

    AgentTurn &currentTurn()
    {
        Q_ASSERT(!m_turns.isEmpty());
        return m_turns.last();
    }

    void beginTurn(const QString &userMessage)
    {
        AgentTurn turn;
        turn.userMessage = userMessage;
        m_turns.append(turn);

        // Add to conversation history
        m_messages.append({AgentMessage::Role::User, userMessage});
    }

    void addStep(const AgentStep &step)
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().steps.append(step);

        // Update conversation history based on step type
        if (step.type == AgentStep::Type::ModelCall) {
            AgentMessage msg;
            msg.role    = AgentMessage::Role::Assistant;
            msg.content = step.modelResponse;
            m_messages.append(msg);
        } else if (step.type == AgentStep::Type::ToolCall) {
            // Record tool call in assistant message
            AgentMessage assistMsg;
            assistMsg.role = AgentMessage::Role::Assistant;
            assistMsg.toolCalls.append(step.toolCall);
            m_messages.append(assistMsg);

            // Record tool result
            AgentMessage toolMsg;
            toolMsg.role       = AgentMessage::Role::Tool;
            toolMsg.toolCallId = step.toolCall.id;
            toolMsg.content    = step.toolResult;
            m_messages.append(toolMsg);
        } else if (step.type == AgentStep::Type::FinalAnswer) {
            AgentMessage msg;
            msg.role    = AgentMessage::Role::Assistant;
            msg.content = step.finalText;
            m_messages.append(msg);

            // Accumulate patches
            m_turns.last().patchProposals.append(step.patches);
        }
    }

    // ── Conversation history (for model context) ──────────────────────────
    const QList<AgentMessage> &messages() const { return m_messages; }

    /// Pre-populate conversation history (e.g. when restoring a saved session).
    void setMessages(const QList<AgentMessage> &msgs) { m_messages = msgs; }

    // ── All patches across all turns ──────────────────────────────────────
    QList<PatchProposal> allPatches() const
    {
        QList<PatchProposal> all;
        for (const auto &turn : m_turns)
            all.append(turn.patchProposals);
        return all;
    }

    // ── File change tracking ──────────────────────────────────────────────
    void recordFileChange(const QString &filePath, const QString &beforeContent)
    {
        if (!m_fileSnapshots.contains(filePath))
            m_fileSnapshots[filePath] = beforeContent;
    }

    QHash<QString, QString> fileSnapshots() const { return m_fileSnapshots; }

    // ── Summary ───────────────────────────────────────────────────────────
    int turnCount() const { return m_turns.size(); }

    int totalSteps() const
    {
        int n = 0;
        for (const auto &t : m_turns)
            n += t.steps.size();
        return n;
    }

    int totalToolCalls() const
    {
        int n = 0;
        for (const auto &t : m_turns)
            for (const auto &s : t.steps)
                if (s.type == AgentStep::Type::ToolCall)
                    ++n;
        return n;
    }

private:
    QString              m_id;
    QDateTime            m_createdAt;
    bool                 m_agentMode = false;
    QString              m_providerId;
    QString              m_model;

    QList<AgentTurn>     m_turns;
    QList<AgentMessage>  m_messages;   // running convo for model context

    // file path → original content (before agent touched it)
    QHash<QString, QString> m_fileSnapshots;
};
