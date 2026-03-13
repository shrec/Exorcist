#include "reviewmanager.h"

#include <QRegularExpression>

ReviewManager::ReviewManager(QObject *parent)
    : QObject(parent)
{
}

QString ReviewManager::buildSelectionReviewContext(const QString &code,
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

QString ReviewManager::buildGitReviewContext(const QString &diff,
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

QList<ReviewComment> ReviewManager::parseReviewResponse(const QString &response) const
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
            if (!suggestionCode.isEmpty())
                suggestionCode += QLatin1Char('\n');
            suggestionCode += line;
            continue;
        }

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

    if (!current.message.isEmpty())
        comments.append(current);

    return comments;
}

QString ReviewManager::reviewSystemPrompt()
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

void ReviewManager::parseCommentLine(const QString &line, ReviewComment &comment)
{
    static const QRegularExpression lineRx(
        QStringLiteral(R"((?:Line\s+|:)(\d+))"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = lineRx.match(line);
    if (match.hasMatch())
        comment.line = match.captured(1).toInt();

    static const QRegularExpression msgRx(
        QStringLiteral(R"(\*\*\s*(?:Line\s+\d+)?:?\s*(.+))"));
    const auto msgMatch = msgRx.match(line);
    if (msgMatch.hasMatch())
        comment.message = msgMatch.captured(1).trimmed();
    else
        comment.message = line.trimmed();
}
