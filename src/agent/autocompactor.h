#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class AgentController;

class AutoCompactor : public QObject
{
    Q_OBJECT
public:
    explicit AutoCompactor(QObject *parent = nullptr);

    void setController(AgentController *ctrl) { m_controller = ctrl; }
    void setThresholdRatio(double ratio) { m_thresholdRatio = ratio; }
    void setMaxContextTokens(int max) { m_maxContextTokens = max; }

    // Check if compaction is needed based on current history size
    bool shouldCompact(const QJsonArray &history, int totalTokens) const;

    // Perform compaction: summarize oldest N messages, keeping recent ones
    QJsonArray compact(const QJsonArray &history) const;

    // Estimate token count of a messages array (rough: chars / 4)
    static int estimateTokens(const QJsonArray &messages);

signals:
    void compactionPerformed(int removedMessages, int savedTokens);

private:
    AgentController *m_controller = nullptr;
    double m_thresholdRatio = 0.75;  // compact when > 75% of max
    int m_maxContextTokens = 120000;
};
