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
    explicit ChatSessionModel(QObject *parent = nullptr);

    // ── Identity ──────────────────────────────────────────────────────────
    QString id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }
    QDateTime createdAt() const { return m_createdAt; }

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    // ── Mode / Model ──────────────────────────────────────────────────────
    int mode() const { return m_mode; }
    void setMode(int mode);

    QString selectedModel() const { return m_selectedModel; }
    void setSelectedModel(const QString &model);

    QString providerId() const { return m_providerId; }
    void setProviderId(const QString &id) { m_providerId = id; }

    // ── Turns ─────────────────────────────────────────────────────────────
    int turnCount() const { return m_turns.size(); }
    const QList<ChatTurnModel> &turns() const { return m_turns; }
    const ChatTurnModel &turn(int index) const { return m_turns[index]; }

    bool isEmpty() const { return m_turns.isEmpty(); }

    void beginTurn(const QString &userMessage,
                   const QStringList &attachments = {},
                   int mode = 0,
                   const QString &modelId = {},
                   const QString &providerId = {},
                   const QString &slashCommand = {});

    ChatTurnModel &currentTurn();

    void appendPart(const ChatContentPart &part);
    void appendMarkdownDelta(const QString &delta);
    void appendThinkingDelta(const QString &delta);

    void updateToolState(const QString &callId,
                         ChatContentPart::ToolState state,
                         const QString &output = {});

    void completeTurn();
    void errorTurn(const QString &errorText);
    void cancelTurn();

    void setTurnFeedback(int index, ChatTurnModel::Feedback fb);
    void clear();

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
