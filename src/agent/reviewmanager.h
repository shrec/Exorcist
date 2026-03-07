#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
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
    explicit ReviewManager(QObject *parent = nullptr) : QObject(parent) {}

    // Build review context for selection
    QString buildSelectionReviewContext(const QString &code,
                                        const QString &filePath,
                                        const QString &languageId) const
    {
        return QStringLiteral(
            "Review the following %1 code from `%2`:\n\n```%1\n%3\n```\n\n"
            "Provide specific, actionable feedback on:\n"
            "- Bugs or logic errors\n"
            "- Security vulnerabilities\n"
            "- Performance issues\n"
            "- Code style and best practices\n"
            "- Potential improvements\n\n"
            "Format each issue as:\n"
            "**[SEVERITY]** Line N: description\n"
            "```suggestion\nfixed code\n```")
            .arg(languageId, filePath, code);
    }

    // Build review context for git changes
    QString buildGitReviewContext(const QString &diff,
                                  const QStringList &changedFiles) const
    {
        QString fileList;
        for (const auto &f : changedFiles)
            fileList += QStringLiteral("- %1\n").arg(f);

        return QStringLiteral(
            "Review the following git changes:\n\n"
            "**Changed files:**\n%1\n"
            "**Diff:**\n```diff\n%2\n```\n\n"
            "Focus on:\n"
            "- Bugs introduced in the changes\n"
            "- Security issues\n"
            "- Missing error handling\n"
            "- Code quality concerns\n\n"
            "Format each issue as:\n"
            "**[SEVERITY]** file:line — description\n"
            "```suggestion\nfixed code\n```")
            .arg(fileList, diff.left(12000));
    }

    // Parse model response into structured comments
    QList<ReviewComment> parseReviewResponse(const QString &response) const
    {
        QList<ReviewComment> comments;
        const QStringList lines = response.split(QLatin1Char('\n'));

        ReviewComment current;
        bool inSuggestion = false;
        QString suggestionCode;

        for (const auto &line : lines) {
            if (line.startsWith(QStringLiteral("```suggestion"))) {
                inSuggestion = true;
                suggestionCode.clear();
                continue;
            }
            if (inSuggestion && line.trimmed() == QStringLiteral("```")) {
                inSuggestion = false;
                current.suggestedCode = suggestionCode.trimmed();
                if (!current.message.isEmpty()) {
                    comments.append(current);
                    current = ReviewComment();
                }
                continue;
            }
            if (inSuggestion) {
                if (!suggestionCode.isEmpty()) suggestionCode += QLatin1Char('\n');
                suggestionCode += line;
                continue;
            }

            // Parse severity + line: **[ERROR]** Line 42: message
            if (line.contains(QStringLiteral("[ERROR]"))) {
                current.severity = QStringLiteral("error");
                parseCommentLine(line, current);
            } else if (line.contains(QStringLiteral("[WARNING]"))) {
                current.severity = QStringLiteral("warning");
                parseCommentLine(line, current);
            } else if (line.contains(QStringLiteral("[INFO]"))) {
                current.severity = QStringLiteral("suggestion");
                parseCommentLine(line, current);
            }
        }
        // Flush last if no suggestion followed
        if (!current.message.isEmpty())
            comments.append(current);

        return comments;
    }

    // System prompt for code review
    static QString reviewSystemPrompt()
    {
        return QStringLiteral(
            "You are a senior code reviewer. Review the given code carefully.\n"
            "For each issue found, specify:\n"
            "1. Severity: [ERROR], [WARNING], or [INFO]\n"
            "2. The line number\n"
            "3. A clear description of the issue\n"
            "4. Optionally, a ```suggestion block with fixed code\n\n"
            "Be concise and actionable. Focus on real issues, not style nitpicks.");
    }

signals:
    void reviewCompleted(const QList<ReviewComment> &comments);

private:
    static void parseCommentLine(const QString &line, ReviewComment &comment)
    {
        // Try to extract line number: "Line 42:" or "file.cpp:42"
        static const QRegularExpression lineRx(QStringLiteral(R"((?:Line\s+|:)(\d+))"),
                                                QRegularExpression::CaseInsensitiveOption);
        const auto match = lineRx.match(line);
        if (match.hasMatch())
            comment.line = match.captured(1).toInt();

        // Extract message after the severity tag
        static const QRegularExpression msgRx(QStringLiteral(R"(\*\*\s*(?:Line\s+\d+)?:?\s*(.+))"));
        const auto msgMatch = msgRx.match(line);
        if (msgMatch.hasMatch())
            comment.message = msgMatch.captured(1).trimmed();
        else
            comment.message = line.trimmed();
    }
};
