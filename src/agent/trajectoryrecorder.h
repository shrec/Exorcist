#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// TrajectoryRecorder — records multi-step agent reasoning to JSON.
//
// Captures each turn of an agent conversation: user messages, tool calls,
// tool results, assistant responses. Can be exported and replayed.
// ─────────────────────────────────────────────────────────────────────────────

struct TrajectoryStep {
    enum class Type { UserMessage, AssistantMessage, ToolCall, ToolResult, SystemEvent };

    Type type;
    QDateTime timestamp;
    QString content;       // message text or tool call JSON
    QString toolName;      // for ToolCall/ToolResult
    QJsonObject metadata;  // additional data (token count, model, etc.)

    QJsonObject toJson() const;
};

class TrajectoryRecorder : public QObject
{
    Q_OBJECT

public:
    explicit TrajectoryRecorder(QObject *parent = nullptr);

    void startRecording(const QString &sessionId);
    void stopRecording();
    bool isRecording() const { return m_recording; }

    void recordStep(TrajectoryStep::Type type,
                    const QString &content,
                    const QString &toolName = {},
                    const QJsonObject &metadata = {});

    int stepCount() const { return m_steps.size(); }
    TrajectoryStep step(int index) const;
    QJsonObject toJson() const;
    QString toJsonString() const;
    QList<TrajectoryStep> steps() const { return m_steps; }

signals:
    void stepRecorded(int totalSteps);

private:
    bool m_recording = false;
    QString m_sessionId;
    QDateTime m_startTime;
    QList<TrajectoryStep> m_steps;
};
