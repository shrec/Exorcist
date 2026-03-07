#pragma once

#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// ReviewComment — a single code review suggestion
// ─────────────────────────────────────────────────────────────────────────────

struct ReviewComment {
    int line = 0;           // 1-based line number
    int endLine = 0;        // end line for range comments
    QString severity;       // "suggestion", "warning", "error"
    QString message;        // review comment text
    QString suggestedCode;  // optional code suggestion
    bool applied = false;
    bool dismissed = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// InlineReviewWidget — shows AI review comments as gutter annotations
//
// Attaches to an EditorView and displays review suggestions inline.
// Each comment can be applied, dismissed, or discussed in chat.
// ─────────────────────────────────────────────────────────────────────────────

class InlineReviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InlineReviewWidget(QWidget *parent = nullptr);

    /// Set review comments for the current file
    void setComments(const QList<ReviewComment> &comments);

    /// Get all comments
    QList<ReviewComment> comments() const { return m_comments; }

    /// Get active (non-dismissed) comment count
    int activeCount() const;

    /// Navigate to next/previous comment
    void nextComment();
    void prevComment();

    /// Show the widget anchored to a specific line in the editor
    void showAtLine(int line, QPlainTextEdit *editor);

signals:
    void applyClicked(int commentIndex, const QString &suggestedCode);
    void dismissClicked(int commentIndex);
    void continueInChat(int commentIndex, const QString &message);
    void navigateToLine(int line);

private:
    void buildUI();
    void updateDisplay();

    QVBoxLayout *m_layout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_counterLabel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;

    QList<ReviewComment> m_comments;
    int m_currentIndex = 0;
};
