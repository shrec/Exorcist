#include "autocompactor.h"

#include <QJsonDocument>

AutoCompactor::AutoCompactor(QObject *parent)
    : QObject(parent)
{
}

bool AutoCompactor::shouldCompact(const QJsonArray &history, int totalTokens) const
{
    Q_UNUSED(history)
    return totalTokens > static_cast<int>(m_maxContextTokens * m_thresholdRatio);
}

int AutoCompactor::estimateTokens(const QJsonArray &messages)
{
    int chars = 0;
    for (const QJsonValue &val : messages) {
        const QJsonObject msg = val.toObject();
        chars += msg.value(QStringLiteral("content")).toString().size();
        // Tool results can be in nested arrays
        const QJsonArray parts = msg.value(QStringLiteral("parts")).toArray();
        for (const QJsonValue &p : parts)
            chars += p.toObject().value(QStringLiteral("text")).toString().size();
    }
    return chars / 4; // rough token estimate
}

QJsonArray AutoCompactor::compact(const QJsonArray &history) const
{
    if (history.size() <= 4)
        return history; // too small to compact

    // Keep system message (index 0), summarize middle, keep last 4 messages
    const int keepRecent = 4;
    const int keepStart = 1;  // after system message
    const int summarizeEnd = history.size() - keepRecent;

    if (summarizeEnd <= keepStart)
        return history;

    // Build summary of compacted messages
    QString summary;
    summary += QStringLiteral("## Conversation Summary\n\n");

    int userCount = 0, assistantCount = 0, toolCount = 0;
    QStringList topics;

    for (int i = keepStart; i < summarizeEnd; ++i) {
        const QJsonObject msg = history[i].toObject();
        const QString role = msg.value(QStringLiteral("role")).toString();
        const QString content = msg.value(QStringLiteral("content")).toString();

        if (role == QStringLiteral("user")) {
            ++userCount;
            // Extract first line as topic hint
            const int nl = content.indexOf(QLatin1Char('\n'));
            const QString firstLine = (nl > 0) ? content.left(nl).trimmed() : content.trimmed();
            if (!firstLine.isEmpty() && firstLine.size() < 200)
                topics.append(firstLine);
        } else if (role == QStringLiteral("assistant")) {
            ++assistantCount;
        } else if (role == QStringLiteral("tool")) {
            ++toolCount;
        }
    }

    summary += QStringLiteral("Compacted %1 messages (%2 user, %3 assistant, %4 tool calls).\n\n")
               .arg(summarizeEnd - keepStart).arg(userCount).arg(assistantCount).arg(toolCount);

    if (!topics.isEmpty()) {
        summary += QStringLiteral("### Topics Discussed\n");
        for (const QString &t : std::as_const(topics))
            summary += QStringLiteral("- ") + t + QStringLiteral("\n");
    }

    // Build new history
    QJsonArray result;

    // System message
    result.append(history[0]);

    // Compacted summary as a system-ish user message
    QJsonObject summaryMsg;
    summaryMsg[QStringLiteral("role")] = QStringLiteral("user");
    summaryMsg[QStringLiteral("content")] = summary;
    result.append(summaryMsg);

    QJsonObject ackMsg;
    ackMsg[QStringLiteral("role")] = QStringLiteral("assistant");
    ackMsg[QStringLiteral("content")] = QStringLiteral(
        "Understood. I have the conversation summary above and will continue from here.");
    result.append(ackMsg);

    // Recent messages
    for (int i = summarizeEnd; i < history.size(); ++i)
        result.append(history[i]);

    const int before = estimateTokens(history);
    const int after = estimateTokens(result);
    const int removedCount = summarizeEnd - keepStart;

    emit const_cast<AutoCompactor *>(this)->compactionPerformed(removedCount, before - after);

    return result;
}
