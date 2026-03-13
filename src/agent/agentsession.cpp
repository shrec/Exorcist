#include "agentsession.h"

AgentSession::AgentSession()
    : m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_createdAt(QDateTime::currentDateTime())
{}

void AgentSession::beginTurn(const QString &userMessage)
{
    AgentTurn turn;
    turn.userMessage = userMessage;
    m_turns.append(turn);

    m_messages.append({AgentMessage::Role::User, userMessage});
}

void AgentSession::addStep(const AgentStep &step)
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().steps.append(step);

    if (step.type == AgentStep::Type::ModelCall) {
        AgentMessage msg;
        msg.role    = AgentMessage::Role::Assistant;
        msg.content = step.modelResponse;
        m_messages.append(msg);
    } else if (step.type == AgentStep::Type::ToolCall) {
        AgentMessage assistMsg;
        assistMsg.role = AgentMessage::Role::Assistant;
        assistMsg.toolCalls.append(step.toolCall);
        m_messages.append(assistMsg);

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

        m_turns.last().patchProposals.append(step.patches);
    }
}

QList<PatchProposal> AgentSession::allPatches() const
{
    QList<PatchProposal> all;
    for (const auto &turn : m_turns)
        all.append(turn.patchProposals);
    return all;
}

void AgentSession::recordFileChange(const QString &filePath, const QString &beforeContent)
{
    if (!m_fileSnapshots.contains(filePath))
        m_fileSnapshots[filePath] = beforeContent;
}

int AgentSession::totalSteps() const
{
    int n = 0;
    for (const auto &t : m_turns)
        n += t.steps.size();
    return n;
}

int AgentSession::totalToolCalls() const
{
    int n = 0;
    for (const auto &t : m_turns)
        for (const auto &s : t.steps)
            if (s.type == AgentStep::Type::ToolCall)
                ++n;
    return n;
}
