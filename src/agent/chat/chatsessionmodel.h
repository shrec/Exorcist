#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUuid>

#include "chatturnmodel.h"

// ── ChatSessionModel ──────────────────────────────────────────────────────────
//
// Holds the state for one chat conversation, including all turns, metadata,
// and streaming state. This is the single source of truth for the transcript
// and input state.
//
// Signals notify the UI layer of changes; widgets never mutate model data
// directly — they call model methods which emit appropriate signals.

class ChatSessionModel : public QObject
{
    Q_OBJECT

public:
    explicit ChatSessionModel(QObject *parent = nullptr)
        : QObject(parent)
        , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_createdAt(QDateTime::currentDateTime())
    {}

    // ── Identity ──────────────────────────────────────────────────────────
    QString id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }
    QDateTime createdAt() const { return m_createdAt; }

    QString title() const { return m_title; }
    void setTitle(const QString &title)
    {
        if (m_title != title) {
            m_title = title;
            emit titleChanged(title);
        }
    }

    // ── Mode / Model ──────────────────────────────────────────────────────
    int mode() const { return m_mode; }
    void setMode(int mode)
    {
        if (m_mode != mode) {
            m_mode = mode;
            emit modeChanged(mode);
        }
    }

    QString selectedModel() const { return m_selectedModel; }
    void setSelectedModel(const QString &model)
    {
        if (m_selectedModel != model) {
            m_selectedModel = model;
            emit modelChanged(model);
        }
    }

    QString providerId() const { return m_providerId; }
    void setProviderId(const QString &id) { m_providerId = id; }

    // ── Turns ─────────────────────────────────────────────────────────────
    int turnCount() const { return m_turns.size(); }
    const QList<ChatTurnModel> &turns() const { return m_turns; }
    const ChatTurnModel &turn(int index) const { return m_turns[index]; }

    bool isEmpty() const { return m_turns.isEmpty(); }

    // Begin a new turn (user sends a message)
    void beginTurn(const QString &userMessage,
                   const QStringList &attachments = {},
                   int mode = 0,
                   const QString &modelId = {},
                   const QString &providerId = {},
                   const QString &slashCommand = {})
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

    // Access the current (last) turn for updates during streaming
    ChatTurnModel &currentTurn()
    {
        Q_ASSERT(!m_turns.isEmpty());
        return m_turns.last();
    }

    // Append a content part to the current turn
    void appendPart(const ChatContentPart &part)
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().appendPart(part);
        emit turnUpdated(m_turns.size() - 1);
    }

    // Append a markdown streaming delta
    void appendMarkdownDelta(const QString &delta)
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().appendMarkdownDelta(delta);
        emit turnUpdated(m_turns.size() - 1);
    }

    // Append a thinking streaming delta
    void appendThinkingDelta(const QString &delta)
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().appendThinkingDelta(delta);
        emit turnUpdated(m_turns.size() - 1);
    }

    // Update a tool invocation state
    void updateToolState(const QString &callId,
                         ChatContentPart::ToolState state,
                         const QString &output = {})
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

    // Mark current turn as complete
    void completeTurn()
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().state = ChatTurnModel::State::Complete;
        emit turnUpdated(m_turns.size() - 1);
        emit turnCompleted(m_turns.size() - 1);
    }

    // Mark current turn as error
    void errorTurn(const QString &errorText)
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().state = ChatTurnModel::State::Error;
        m_turns.last().appendPart(ChatContentPart::error(errorText));
        emit turnUpdated(m_turns.size() - 1);
    }

    // Mark current turn as cancelled
    void cancelTurn()
    {
        Q_ASSERT(!m_turns.isEmpty());
        m_turns.last().state = ChatTurnModel::State::Cancelled;
        emit turnUpdated(m_turns.size() - 1);
    }

    // Set feedback on a turn
    void setTurnFeedback(int index, ChatTurnModel::Feedback fb)
    {
        if (index >= 0 && index < m_turns.size()) {
            m_turns[index].feedback = fb;
            emit turnUpdated(index);
        }
    }

    // Clear all turns (new session)
    void clear()
    {
        m_turns.clear();
        m_title.clear();
        m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_createdAt = QDateTime::currentDateTime();
        emit sessionCleared();
    }

signals:
    void turnAdded(int index);
    void turnUpdated(int index);
    void turnCompleted(int index);
    void sessionCleared();
    void titleChanged(const QString &title);
    void modeChanged(int mode);
    void modelChanged(const QString &model);

private:
    QString   m_id;
    QDateTime m_createdAt;
    QString   m_title;
    int       m_mode = 0;           // 0=Ask, 1=Edit, 2=Agent
    QString   m_selectedModel;
    QString   m_providerId;
    QList<ChatTurnModel> m_turns;
};
