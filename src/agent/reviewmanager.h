#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "../editor/inlinereviewwidget.h" // for ReviewComment

// ─────────────────────────────────────────────────────────────────────────────
// ReviewManager — orchestrates code review workflows.
//
// Supports reviewing: selection, file, git changes (staged/unstaged/all).
// Produces a system prompt + context for the AgentController, then parses
// the model's response into structured ReviewComment objects that can be
// shown via InlineReviewWidget.
// ─────────────────────────────────────────────────────────────────────────────

class ReviewManager : public QObject
{
    Q_OBJECT

public:
    explicit ReviewManager(QObject *parent = nullptr);

    QString buildSelectionReviewContext(const QString &code,
                                        const QString &filePath,
                                        const QString &languageId) const;

    QString buildGitReviewContext(const QString &diff,
                                  const QStringList &changedFiles) const;

    QList<ReviewComment> parseReviewResponse(const QString &response) const;

    static QString reviewSystemPrompt();

signals:
    void reviewCompleted(const QList<ReviewComment> &comments);

private:
    static void parseCommentLine(const QString &line, ReviewComment &comment);
};
