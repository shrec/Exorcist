#include "inlinereviewwidget.h"

#include <QApplication>
#include <QPalette>
#include <QTextBlock>
#include <QTextCursor>

#include <memory>

InlineReviewWidget::InlineReviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::ToolTip);
    setMinimumWidth(400);
    setMaximumWidth(600);
    buildUI();
}

void InlineReviewWidget::buildUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);

    // Header
    auto *header = new QHBoxLayout;
    m_titleLabel = new QLabel(tr("Code Review"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px;"));
    header->addWidget(m_titleLabel);
    header->addStretch();

    m_counterLabel = new QLabel(this);
    header->addWidget(m_counterLabel);

    m_prevBtn = new QPushButton(QStringLiteral("\u25B2"), this); // ▲
    m_prevBtn->setFixedSize(24, 24);
    m_prevBtn->setToolTip(tr("Previous comment"));
    connect(m_prevBtn, &QPushButton::clicked, this, &InlineReviewWidget::prevComment);
    header->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton(QStringLiteral("\u25BC"), this); // ▼
    m_nextBtn->setFixedSize(24, 24);
    m_nextBtn->setToolTip(tr("Next comment"));
    connect(m_nextBtn, &QPushButton::clicked, this, &InlineReviewWidget::nextComment);
    header->addWidget(m_nextBtn);

    m_layout->addLayout(header);

    // Scroll area for comments
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMaximumHeight(300);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_contentWidget = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollArea->setWidget(m_contentWidget);
    m_layout->addWidget(m_scrollArea);

    setStyleSheet(QStringLiteral(
        "InlineReviewWidget {"
        "  background: palette(window);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "}"));
}

void InlineReviewWidget::setComments(const QList<ReviewComment> &comments)
{
    m_comments = comments;
    m_currentIndex = 0;
    updateDisplay();
}

int InlineReviewWidget::activeCount() const
{
    int count = 0;
    for (const auto &c : m_comments) {
        if (!c.dismissed && !c.applied)
            ++count;
    }
    return count;
}

void InlineReviewWidget::nextComment()
{
    if (m_comments.isEmpty()) return;
    m_currentIndex = (m_currentIndex + 1) % m_comments.size();
    updateDisplay();
    if (m_currentIndex < m_comments.size())
        emit navigateToLine(m_comments[m_currentIndex].line);
}

void InlineReviewWidget::prevComment()
{
    if (m_comments.isEmpty()) return;
    m_currentIndex = (m_currentIndex - 1 + m_comments.size()) % m_comments.size();
    updateDisplay();
    if (m_currentIndex < m_comments.size())
        emit navigateToLine(m_comments[m_currentIndex].line);
}

void InlineReviewWidget::showAtLine(int line, QPlainTextEdit *editor)
{
    if (!editor) return;

    // Find the comment at this line
    for (int i = 0; i < m_comments.size(); ++i) {
        if (m_comments[i].line == line) {
            m_currentIndex = i;
            break;
        }
    }

    // Position below the line in the editor
    QTextCursor cursor(editor->document()->findBlockByLineNumber(line - 1));
    const QRect rect = editor->cursorRect(cursor);
    const QPoint pos = editor->mapToGlobal(rect.bottomLeft());
    move(pos.x(), pos.y() + 4);

    updateDisplay();
    show();
}

void InlineReviewWidget::updateDisplay()
{
    // Clear old widgets
    while (m_contentLayout->count() > 0) {
        auto *item = m_contentLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        auto guard = std::unique_ptr<QLayoutItem>(item);
    }

    m_counterLabel->setText(QStringLiteral("%1/%2")
                                .arg(m_currentIndex + 1)
                                .arg(m_comments.size()));

    if (m_comments.isEmpty()) {
        m_contentLayout->addWidget(new QLabel(tr("No review comments"), m_contentWidget));
        return;
    }

    const auto &comment = m_comments[m_currentIndex];

    // Severity icon + message
    QString icon;
    if (comment.severity == QStringLiteral("error"))
        icon = QStringLiteral("\u274C "); // ❌
    else if (comment.severity == QStringLiteral("warning"))
        icon = QStringLiteral("\u26A0 "); // ⚠
    else
        icon = QStringLiteral("\U0001F4A1 "); // 💡

    auto *msgLabel = new QLabel(icon + comment.message, m_contentWidget);
    msgLabel->setWordWrap(true);
    msgLabel->setTextFormat(Qt::PlainText);
    m_contentLayout->addWidget(msgLabel);

    // Line info
    auto *lineLabel = new QLabel(
        tr("Line %1").arg(comment.line) +
            (comment.endLine > comment.line
                 ? QStringLiteral("-%1").arg(comment.endLine)
                 : QString()),
        m_contentWidget);
    lineLabel->setStyleSheet(QStringLiteral("color: palette(mid); font-size: 11px;"));
    m_contentLayout->addWidget(lineLabel);

    // Suggested code preview
    if (!comment.suggestedCode.isEmpty()) {
        auto *codeLabel = new QLabel(m_contentWidget);
        codeLabel->setTextFormat(Qt::PlainText);
        codeLabel->setText(comment.suggestedCode);
        codeLabel->setStyleSheet(QStringLiteral(
            "background: palette(base); padding: 4px; font-family: monospace;"
            " border: 1px solid palette(mid); border-radius: 3px;"));
        codeLabel->setWordWrap(true);
        m_contentLayout->addWidget(codeLabel);
    }

    // Action buttons
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(0, 4, 0, 0);

    if (!comment.suggestedCode.isEmpty() && !comment.applied) {
        auto *applyBtn = new QPushButton(tr("Apply"), m_contentWidget);
        applyBtn->setToolTip(tr("Apply this suggestion"));
        const int idx = m_currentIndex;
        connect(applyBtn, &QPushButton::clicked, this, [this, idx] {
            m_comments[idx].applied = true;
            emit applyClicked(idx, m_comments[idx].suggestedCode);
            updateDisplay();
        });
        btnLayout->addWidget(applyBtn);
    }

    if (comment.applied) {
        auto *appliedLabel = new QLabel(QStringLiteral("\u2705 Applied"), m_contentWidget);
        appliedLabel->setStyleSheet(QStringLiteral("color: green;"));
        btnLayout->addWidget(appliedLabel);
    }

    if (!comment.dismissed && !comment.applied) {
        auto *dismissBtn = new QPushButton(tr("Dismiss"), m_contentWidget);
        const int idx = m_currentIndex;
        connect(dismissBtn, &QPushButton::clicked, this, [this, idx] {
            m_comments[idx].dismissed = true;
            emit dismissClicked(idx);
            updateDisplay();
        });
        btnLayout->addWidget(dismissBtn);

        auto *chatBtn = new QPushButton(tr("Discuss"), m_contentWidget);
        chatBtn->setToolTip(tr("Continue in chat"));
        connect(chatBtn, &QPushButton::clicked, this, [this, idx] {
            emit continueInChat(idx, m_comments[idx].message);
        });
        btnLayout->addWidget(chatBtn);
    }

    btnLayout->addStretch();
    m_contentLayout->addLayout(btnLayout);
}
