#pragma once

#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatsessionmodel.h"
#include "chatthemetokens.h"
#include "chatturnmodel.h"
#include "chatturnwidget.h"

// ── ChatTranscriptView ───────────────────────────────────────────────────────
//
// A scroll-area that vertically stacks ChatTurnWidget instances — one per
// request/response pair.  Listens to ChatSessionModel signals and keeps the
// widget tree in sync.  Auto-scrolls to bottom during streaming responses.

class ChatTranscriptView : public QScrollArea
{
    Q_OBJECT

public:
    explicit ChatTranscriptView(ChatSessionModel *model, QWidget *parent = nullptr);

    ChatTurnWidget *turnWidget(int index) const;
    void scrollToBottom();
    void setStreamingActive(bool active);

    void appendMarkdownDelta(int turnIndex, const QString &delta);
    void appendThinkingDelta(int turnIndex, const QString &delta);
    void addContentPart(int turnIndex, const ChatContentPart &part);
    void updateToolState(int turnIndex, const QString &callId,
                         const ChatContentPart &part);

signals:
    void followupClicked(const QString &message);
    void feedbackGiven(const QString &turnId, bool helpful);
    void toolConfirmed(const QString &callId, int approval);
    void fileClicked(const QString &path);
    void codeActionRequested(const QUrl &url);
    void insertCodeRequested(const QString &code);
    void retryRequested(const QString &turnId);
    void signInRequested();
    void keepFileRequested(int fileIndex, const QString &path);
    void undoFileRequested(int fileIndex, const QString &path);
    void keepAllRequested();
    void undoAllRequested();

private slots:
    void onTurnAdded(int index);
    void onTurnUpdated(int index);
    void onTurnCompleted(int index);
    void onSessionCleared();

private:
    void createTurnWidget(int index);
    void autoScroll();

    bool    m_streaming        = false;
    bool    m_userScrolledAway = false;
    QTimer *m_scrollTimer      = nullptr;
    ChatSessionModel                *m_model     = nullptr;
    QWidget                         *m_container = nullptr;
    QVBoxLayout                     *m_layout    = nullptr;
    QList<ChatTurnWidget *>          m_turnWidgets;
};
