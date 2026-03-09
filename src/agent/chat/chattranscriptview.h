#pragma once

#include <QMap>
#include <QScrollArea>
#include <QScrollBar>
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
    explicit ChatTranscriptView(ChatSessionModel *model, QWidget *parent = nullptr)
        : QScrollArea(parent)
        , m_model(model)
    {
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
        setStyleSheet(
            QStringLiteral("QScrollArea { background:%1; border:none; }"
                          "QScrollBar:vertical {"
                          "  background:%2; width:8px; border:none;"
                          "}"
                          "QScrollBar::handle:vertical {"
                          "  background:%3; border-radius:4px; min-height:30px;"
                          "}"
                          "QScrollBar::handle:vertical:hover {"
                          "  background:%4;"
                          "}"
                          "QScrollBar::handle:vertical:pressed {"
                          "  background:%5;"
                          "}"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                          "  height:0; background:none;"
                          "}")
                .arg(ChatTheme::PanelBg, ChatTheme::ScrollTrack,
                     ChatTheme::ScrollThumb, ChatTheme::ScrollHandleHover,
                     ChatTheme::ScrollHandlePressed));

        m_container = new QWidget(this);
        m_layout = new QVBoxLayout(m_container);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(2);
        m_layout->addStretch();  // keeps turns top-aligned
        setWidget(m_container);

        // Connect to model signals
        connect(m_model, &ChatSessionModel::turnAdded,
                this, &ChatTranscriptView::onTurnAdded);
        connect(m_model, &ChatSessionModel::turnUpdated,
                this, &ChatTranscriptView::onTurnUpdated);
        connect(m_model, &ChatSessionModel::turnCompleted,
                this, &ChatTranscriptView::onTurnCompleted);
        connect(m_model, &ChatSessionModel::sessionCleared,
                this, &ChatTranscriptView::onSessionCleared);

        // Build widgets for existing turns
        for (int i = 0; i < m_model->turnCount(); ++i)
            createTurnWidget(i);
    }

    ChatTurnWidget *turnWidget(int index) const
    {
        if (index >= 0 && index < m_turnWidgets.size())
            return m_turnWidgets[index];
        return nullptr;
    }

    void scrollToBottom()
    {
        QTimer::singleShot(0, this, [this]() {
            verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        });
    }

    // ── Streaming convenience ─────────────────────────────────────────

    void appendMarkdownDelta(int turnIndex, const QString &delta)
    {
        if (auto *w = turnWidget(turnIndex)) {
            w->appendMarkdownDelta(delta);
            autoScroll();
        }
    }

    void appendThinkingDelta(int turnIndex, const QString &delta)
    {
        if (auto *w = turnWidget(turnIndex)) {
            w->appendThinkingDelta(delta);
            autoScroll();
        }
    }

    void addContentPart(int turnIndex, const ChatContentPart &part)
    {
        if (auto *w = turnWidget(turnIndex)) {
            w->addContentPart(part);
            autoScroll();
        }
    }

    void updateToolState(int turnIndex, const QString &callId,
                         const ChatContentPart &part)
    {
        if (auto *w = turnWidget(turnIndex))
            w->updateToolState(callId, part);
    }

signals:
    void followupClicked(const QString &message);
    void feedbackGiven(const QString &turnId, bool helpful);
    void toolConfirmed(const QString &callId, bool allowed);
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
    void onTurnAdded(int index)
    {
        createTurnWidget(index);
        scrollToBottom();
    }

    void onTurnUpdated(int index)
    {
        // The model has been updated — currently the transcript view pushes
        // streaming deltas directly via the convenience methods above.
        // This signal is available for future use (e.g., re-rendering a turn).
        Q_UNUSED(index)
    }

    void onTurnCompleted(int index)
    {
        if (auto *w = turnWidget(index)) {
            const auto &turn = m_model->turn(index);
            w->finishTurn(turn.state);
        }
    }

    void onSessionCleared()
    {
        for (auto *w : m_turnWidgets)
            w->deleteLater();
        m_turnWidgets.clear();
    }

private:
    void createTurnWidget(int index)
    {
        const auto &turn = m_model->turn(index);
        auto *w = new ChatTurnWidget(turn, m_container);

        connect(w, &ChatTurnWidget::followupClicked,
                this, &ChatTranscriptView::followupClicked);
        connect(w, &ChatTurnWidget::feedbackGiven,
                this, &ChatTranscriptView::feedbackGiven);
        connect(w, &ChatTurnWidget::toolConfirmed,
                this, &ChatTranscriptView::toolConfirmed);
        connect(w, &ChatTurnWidget::fileClicked,
                this, &ChatTranscriptView::fileClicked);
        connect(w, &ChatTurnWidget::codeActionRequested,
                this, &ChatTranscriptView::codeActionRequested);
        connect(w, &ChatTurnWidget::insertCodeRequested,
                this, &ChatTranscriptView::insertCodeRequested);
        connect(w, &ChatTurnWidget::retryRequested,
                this, &ChatTranscriptView::retryRequested);
        connect(w, &ChatTurnWidget::signInRequested,
                this, &ChatTranscriptView::signInRequested);
        connect(w, &ChatTurnWidget::keepFileRequested,
                this, &ChatTranscriptView::keepFileRequested);
        connect(w, &ChatTurnWidget::undoFileRequested,
                this, &ChatTranscriptView::undoFileRequested);
        connect(w, &ChatTurnWidget::keepAllRequested,
                this, &ChatTranscriptView::keepAllRequested);
        connect(w, &ChatTurnWidget::undoAllRequested,
                this, &ChatTranscriptView::undoAllRequested);

        // Insert before the stretch
        int insertPos = m_layout->count() - 1;
        if (insertPos < 0)
            insertPos = 0;
        m_layout->insertWidget(insertPos, w);

        if (index < m_turnWidgets.size())
            m_turnWidgets.insert(index, w);
        else
            m_turnWidgets.append(w);
    }

    void autoScroll()
    {
        auto *sb = verticalScrollBar();
        // Auto-scroll only if the user is near the bottom already
        if (sb->value() >= sb->maximum() - 100)
            scrollToBottom();
    }

    ChatSessionModel                *m_model     = nullptr;
    QWidget                         *m_container = nullptr;
    QVBoxLayout                     *m_layout    = nullptr;
    QList<ChatTurnWidget *>          m_turnWidgets;
};
