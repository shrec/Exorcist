#include "chatsessionmodel.h"

ChatSessionModel::ChatSessionModel(QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_createdAt(QDateTime::currentDateTime())
{
}

void ChatSessionModel::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged(title);
    }
}

void ChatSessionModel::setMode(int mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
    }
}

void ChatSessionModel::setSelectedModel(const QString &model)
{
    if (m_selectedModel != model) {
        m_selectedModel = model;
        emit modelChanged(model);
    }
}

void ChatSessionModel::beginTurn(const QString &userMessage,
                                  const QStringList &attachments,
                                  int mode,
                                  const QString &modelId,
                                  const QString &providerId,
                                  const QString &slashCommand)
{
    ChatTurnModel t;
    t.userMessage = userMessage;
    t.slashCommand = slashCommand;
    t.attachmentNames = attachments;
    t.mode = mode;
    t.modelId = modelId;
    t.providerId = providerId;
    m_turns.append(t);
    emit turnAdded(m_turns.size() - 1);
}

ChatTurnModel &ChatSessionModel::currentTurn()
{
    Q_ASSERT(!m_turns.isEmpty());
    return m_turns.last();
}

void ChatSessionModel::appendPart(const ChatContentPart &part)
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().appendPart(part);
    emit turnUpdated(m_turns.size() - 1);
}

void ChatSessionModel::appendMarkdownDelta(const QString &delta)
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().appendMarkdownDelta(delta);
    emit turnUpdated(m_turns.size() - 1);
}

void ChatSessionModel::appendThinkingDelta(const QString &delta)
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().appendThinkingDelta(delta);
    emit turnUpdated(m_turns.size() - 1);
}

void ChatSessionModel::updateToolState(const QString &callId,
                                        ChatContentPart::ToolState state,
                                        const QString &output)
{
    Q_ASSERT(!m_turns.isEmpty());
    auto *part = m_turns.last().findToolPart(callId);
    if (part) {
        part->toolState = state;
        if (!output.isEmpty())
            part->toolOutput = output;
        emit turnUpdated(m_turns.size() - 1);
    }
}

void ChatSessionModel::completeTurn()
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().state = ChatTurnModel::State::Complete;
    emit turnUpdated(m_turns.size() - 1);
    emit turnCompleted(m_turns.size() - 1);
}

void ChatSessionModel::errorTurn(const QString &errorText)
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().state = ChatTurnModel::State::Error;
    m_turns.last().appendPart(ChatContentPart::error(errorText));
    emit turnUpdated(m_turns.size() - 1);
}

void ChatSessionModel::cancelTurn()
{
    Q_ASSERT(!m_turns.isEmpty());
    m_turns.last().state = ChatTurnModel::State::Cancelled;
    emit turnUpdated(m_turns.size() - 1);
}

void ChatSessionModel::setTurnFeedback(int index, ChatTurnModel::Feedback fb)
{
    if (index >= 0 && index < m_turns.size()) {
        m_turns[index].feedback = fb;
        emit turnUpdated(index);
    }
}

void ChatSessionModel::clear()
{
    m_turns.clear();
    m_title.clear();
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_createdAt = QDateTime::currentDateTime();
    emit sessionCleared();
}
