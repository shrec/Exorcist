#include "chattranscriptview.h"

#include <QScrollBar>

ChatTranscriptView::ChatTranscriptView(ChatSessionModel *model, QWidget *parent)
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
            .arg(ChatTheme::pick(ChatTheme::PanelBg, ChatTheme::L_PanelBg),
                 ChatTheme::pick(ChatTheme::ScrollTrack, ChatTheme::L_ScrollTrack),
                 ChatTheme::pick(ChatTheme::ScrollThumb, ChatTheme::L_ScrollThumb),
                 ChatTheme::pick(ChatTheme::ScrollHandleHover, ChatTheme::L_ScrollHandleHover),
                 ChatTheme::pick(ChatTheme::ScrollHandlePressed, ChatTheme::L_ScrollHandlePressed)));

    m_container = new QWidget(this);
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(0, 4, 0, 4);
    m_layout->setSpacing(4);
    m_layout->addStretch();
    setWidget(m_container);

    connect(m_model, &ChatSessionModel::turnAdded,
            this, &ChatTranscriptView::onTurnAdded);
    connect(m_model, &ChatSessionModel::turnUpdated,
            this, &ChatTranscriptView::onTurnUpdated);
    connect(m_model, &ChatSessionModel::turnCompleted,
            this, &ChatTranscriptView::onTurnCompleted);
    connect(m_model, &ChatSessionModel::sessionCleared,
            this, &ChatTranscriptView::onSessionCleared);

    connect(verticalScrollBar(), &QScrollBar::sliderPressed, this, [this]() {
        if (m_streaming)
            m_userScrolledAway = true;
    });

    for (int i = 0; i < m_model->turnCount(); ++i)
        createTurnWidget(i);
}

ChatTurnWidget *ChatTranscriptView::turnWidget(int index) const
{
    if (index >= 0 && index < m_turnWidgets.size())
        return m_turnWidgets[index];
    return nullptr;
}

void ChatTranscriptView::scrollToBottom()
{
    QTimer::singleShot(0, this, [this]() {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    });
}

void ChatTranscriptView::setStreamingActive(bool active)
{
    m_streaming = active;
    if (active) {
        if (!m_scrollTimer) {
            m_scrollTimer = new QTimer(this);
            m_scrollTimer->setInterval(80);
            connect(m_scrollTimer, &QTimer::timeout, this, [this]() {
                if (!m_streaming) {
                    m_scrollTimer->stop();
                    return;
                }
                if (m_userScrolledAway)
                    return;
                verticalScrollBar()->setValue(verticalScrollBar()->maximum());
            });
        }
        m_userScrolledAway = false;
        m_scrollTimer->start();
    } else {
        if (m_scrollTimer)
            m_scrollTimer->stop();
        QTimer::singleShot(50, this, [this]() {
            if (!m_userScrolledAway)
                verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        });
    }
}

void ChatTranscriptView::appendMarkdownDelta(int turnIndex, const QString &delta)
{
    if (auto *w = turnWidget(turnIndex)) {
        w->appendMarkdownDelta(delta);
        autoScroll();
    }
}

void ChatTranscriptView::appendThinkingDelta(int turnIndex, const QString &delta)
{
    if (auto *w = turnWidget(turnIndex)) {
        w->appendThinkingDelta(delta);
        autoScroll();
    }
}

void ChatTranscriptView::addContentPart(int turnIndex, const ChatContentPart &part)
{
    if (auto *w = turnWidget(turnIndex)) {
        w->addContentPart(part);
        autoScroll();
    }
}

void ChatTranscriptView::updateToolState(int turnIndex, const QString &callId,
                                          const ChatContentPart &part)
{
    if (auto *w = turnWidget(turnIndex)) {
        w->updateToolState(callId, part);
        autoScroll();
    }
}

void ChatTranscriptView::onTurnAdded(int index)
{
    createTurnWidget(index);
    scrollToBottom();
}

void ChatTranscriptView::onTurnUpdated(int index)
{
    Q_UNUSED(index)
}

void ChatTranscriptView::onTurnCompleted(int index)
{
    if (auto *w = turnWidget(index)) {
        const auto &turn = m_model->turn(index);
        w->finishTurn(turn.state);
    }
}

void ChatTranscriptView::onSessionCleared()
{
    for (auto *w : m_turnWidgets)
        w->deleteLater();
    m_turnWidgets.clear();
}

void ChatTranscriptView::createTurnWidget(int index)
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

    int insertPos = m_layout->count() - 1;
    if (insertPos < 0)
        insertPos = 0;
    m_layout->insertWidget(insertPos, w);

    if (index < m_turnWidgets.size())
        m_turnWidgets.insert(index, w);
    else
        m_turnWidgets.append(w);
}

void ChatTranscriptView::autoScroll()
{
    if (m_streaming)
        return;
    QTimer::singleShot(30, this, [this]() {
        auto *sb = verticalScrollBar();
        if (sb->value() >= sb->maximum() - 200)
            sb->setValue(sb->maximum());
    });
}
