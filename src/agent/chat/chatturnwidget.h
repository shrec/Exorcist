#pragma once

#include <QEnterEvent>
#include <QMap>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"
#include "chatturnmodel.h"
#include "aiinterface.h"

class QLabel;
class QToolButton;
class QVBoxLayout;
class ChatFollowupsWidget;
class ChatMarkdownWidget;
class ChatThinkingWidget;
class ChatToolInvocationWidget;
class ChatWorkspaceEditWidget;

// ── ChatTurnWidget ────────────────────────────────────────────────────────────
//
// Renders one request/response pair in the chat transcript.
// Layout:
//   [Avatar]  [User Name]  [Timestamp]
//   User message text
//   ──────────────────────────────────
//   [Avatar]  [Assistant Name]
//   [Content parts laid out vertically...]
//   [Feedback row (thumbs up / thumbs down)]

class ChatTurnWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatTurnWidget(const ChatTurnModel &turn, QWidget *parent = nullptr);

    QString turnId() const { return m_turnId; }

    // ── Streaming updates ─────────────────────────────────────────────
    void appendMarkdownDelta(const QString &delta);
    void appendThinkingDelta(const QString &delta);
    void addContentPart(const ChatContentPart &part);
    void updateToolState(const QString &callId, const ChatContentPart &part);
    void finishTurn(ChatTurnModel::State state);
    void setTokenUsage(int promptTokens, int completionTokens, int totalTokens);
    void showError(const QString &errorText,
                   AgentError::Code code = AgentError::Code::Unknown);

signals:
    void feedbackGiven(const QString &turnId, bool helpful);
    void followupClicked(const QString &message);
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

private:
    void addPartWidget(const ChatContentPart &part);
    static QString feedbackBtnStyle();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

    QString m_turnId;
    bool    m_turnComplete = false;

    QWidget     *m_responseSection = nullptr;
    QWidget     *m_partsContainer  = nullptr;
    QVBoxLayout *m_partsLayout     = nullptr;
    QLabel      *m_modelLabel      = nullptr;
    QWidget     *m_feedbackRow     = nullptr;
    QToolButton *m_thumbUpBtn      = nullptr;
    QToolButton *m_thumbDownBtn    = nullptr;
    QLabel      *m_usageLabel      = nullptr;

    ChatMarkdownWidget  *m_lastMarkdown = nullptr;
    ChatThinkingWidget  *m_lastThinking = nullptr;
    QList<ChatThinkingWidget *> m_thinkingWidgets;
    QMap<QString, ChatToolInvocationWidget *> m_toolWidgets;
};
