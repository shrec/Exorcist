#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// ─────────────────────────────────────────────────────────────────────────────
// BackgroundCompactor — auto-compacts conversations approaching token limits.
//
// Monitors conversation token count. When approaching the limit, triggers
// compaction of older messages while preserving important tool results.
// ─────────────────────────────────────────────────────────────────────────────

class BackgroundCompactor : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundCompactor(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_timer = new QTimer(this);
        m_timer->setInterval(10000); // check every 10s
        connect(m_timer, &QTimer::timeout, this, &BackgroundCompactor::check);
    }

    void start() { m_timer->start(); }
    void stop()  { m_timer->stop(); }

    void setContextLimit(int tokens) { m_contextLimit = tokens; }
    void setThreshold(double ratio) { m_threshold = ratio; }
    void setCurrentTokenCount(int count) { m_currentTokens = count; }

    bool shouldCompact() const
    {
        return m_currentTokens > static_cast<int>(m_contextLimit * m_threshold);
    }

    // Strategy: summarize oldest N messages
    struct CompactionStrategy {
        int messagesToSummarize = 10;
        bool preserveToolResults = true;
        bool preservePinnedContext = true;
        bool keepLastNMessages = 5;
    };

    CompactionStrategy strategy() const { return m_strategy; }
    void setStrategy(const CompactionStrategy &s) { m_strategy = s; }

    // Build compaction prompt
    QString buildCompactionPrompt(const QStringList &messages) const
    {
        QString prompt = QStringLiteral(
            "Summarize the following conversation messages into a concise summary. "
            "Preserve key decisions, code changes, and important context. "
            "Keep tool call results that are still relevant.\n\n");

        const int count = qMin(m_strategy.messagesToSummarize, messages.size());
        for (int i = 0; i < count; ++i)
            prompt += QStringLiteral("---\n%1\n").arg(messages[i]);

        return prompt;
    }

signals:
    void compactionNeeded();
    void compactionCompleted(const QString &summary);

private:
    void check()
    {
        if (shouldCompact())
            emit compactionNeeded();
    }

    QTimer *m_timer;
    int m_contextLimit = 128000;
    double m_threshold = 0.8;
    int m_currentTokens = 0;
    CompactionStrategy m_strategy;
};
